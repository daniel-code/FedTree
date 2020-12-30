//
// Created by liqinbin on 10/14/20.
//
#include "FedTree/FL/FLtrainer.h"
#include "FedTree/Encryption/HE.h"
#include "FedTree/FL/partition.h"
#include "FedTree/FL/comm_helper.h"

void FLtrainer::horizontal_fl_trainer(vector<Party> &parties, Server &server, FLParam &params) {
// Is propose_split_candidates implemented? Should it be a method of TreeBuilder, HistTreeBuilder or server? Shouldnt there be a vector of SplitCandidates returned
//  vector<SplitCandidate> candidates = server.fbuilder.propose_split_candidates();
//  std::tuple <AdditivelyHE::PaillierPublicKey, AdditivelyHE::PaillierPrivateKey> key_pair = server.HE.generate_key_pairs();
//    server.send_info(parties, std::get<0>(keyPairs), candidates);
//    for (int i = 0; i < params.gbdt_param.n_trees; i++){
//        for (j = 0; j < parties.size(); j++){
//            parties[j].update_gradients();
//        }
//        for (int j = 0; j < params.gbdt_param.depth; j++){
//            for (int k = 0; k < parties.size(); k++) {
//                SyncArray<GHPair> hist = parties[j].fbuilder->compute_histogram();
//                if (params.privacy_tech == "he") {
                    // Should HE be a public member of Party?
//                    parties[k].HE.encryption();
//                }
//                if (params.privacy_tech == "dp") {
                    // Should DP be public member of Party?
//                    parties[k].DP.add_gaussian_noise();
//                }
//                parties[k].send_info(hist);
//            }
            // merge_histograms in tree_builder?
//            server.sum_histograms(); // or on Party 1 if using homo encryption
//            server.HE.decrption();
//            if (j != params.gbdt_param.depth - 1) {
//                server.fbuilder.compute_gain();
//                server.fbuilder.get_best_split(); // or using exponential mechanism
//                server.fbuilder.update_tree();
//                server.send_info(); // send split points
//            }
//            else{
//                server.fbuilder.compute_leaf_value();
//                server.DP.add_gaussian_noise(); // for DP: add noises to the tree
//                server.send_info(); // send leaf values
//            }
//        }
//    }
}


void FLtrainer::vertical_fl_trainer(vector<Party> &parties, Server &server, FLParam &params) {

    // load dataset
    GBDTParam &model_param = params.gbdt_param;
    DataSet dataset;
    dataset.load_from_file(model_param.path, params);

    // partition dataset
    int parties_size = parties.size() + 1;
    vector<DataSet> subsets(parties_size);
    Partition partition;
    vector<float> alpha;
    partition.hybrid_partition(dataset, parties_size, alpha, subsets);

    // server and party initialization
    server.init(0, subsets[0], params);
    server.homo_init();
    for (int i = 1; i < parties_size; i++) {
        parties[i - 1].init(i, subsets[i], params);
    }

    // start training
    // for each boosting round
    for (int i = 0; i < params.gbdt_param.n_trees; i++) {
        // Server update, encrypt and send gradients
        server.booster.update_gradients();
        server.booster.encrypt_gradients(server.publicKey);
        for (int j = 0; j < parties.size(); j++) {
            server.send_gradients(parties[j]);
        }
        // for each tree in a round
        for (int k = 0; k < params.gbdt_param.tree_per_rounds; k++) {
            // each party initialize ins2node_id, gradients, etc.
            for (int j = 0; j < parties.size(); j++)
                parties[j].booster.fbuilder->build_init(parties[j].booster.get_gradients(), k);
            // server initialize hist container
            server.booster.fbuilder->parties_hist_init(parties.size());
            // for each level
            for (int l = 0; l < params.gbdt_param.depth; l++) {
                // each party compute hist, send hist to server
                for (int j = 0; j < parties.size(); j++) {
                    parties[j].booster.fbuilder->compute_hist(l);
                    parties[j].send_hist(server);
                }
                // server concat histograms
                server.booster.fbuilder->concat_histograms();
            }
        }
    }
//    parties[0].homo_init();
//    parties[0].send_info();
//    for (int i = 0; i < params.gbdt_param.n_trees; i++){
//        parties[0].update_gradients();
//        parties[0].DP.add_gaussian_noise();
//        parties[0].send_info();
//        for (int j = 0; j < params.gbdt_param.depth; j++){
//            if (j != params.gbdt_param.depth - 1){
//                for (int k = 0; k < parties.size(); k++){
//                    parties[k].fbuilder.propose_split_candidates();
//                    parties[k].fbuilder.compute_histogram();
//                    parties[k].HE.encrypt();
//                }
//                parties[0].decrypt();
//                parties[0].compute_gain();
//                parties[0].get_best_split();
//                parties[0].send_info();
//                parties[z].fbuilder.update_tree();
//                parties[z].send_info()
//            }
//            else{
//                parties[0].fbuilder.compute_leaf_value();
//                parties[0].DP.add_gaussian_noise();
//            }
//        }
//    }
}


void FLtrainer::hybrid_fl_trainer(vector<Party> &parties, Server &server, FLParam &params){
    // todo: initialize parties and server
    int n_party = parties.size();
    Comm comm_helper;
    for(int i = 0; i < params.gbdt_param.n_trees; i++) {
        #pragma omp parallel for
        for (int i = 0; i < n_party; i++) {
            parties[i].booster.boost(parties[i].gbdt.trees);
            comm_helper.send_last_trees_to_server(parties[i], i, server);
        }
        server.merge_trees();
        // todo: send the trees to the party to correct the trees and compute leaf values
        #pragma omp parallel for
        for (int i = 0; i < n_party; i++) {
            comm_helper.send_last_global_trees_to_party(server, parties[i]);
        }
    }

}