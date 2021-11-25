//
// Created by Kelly Yung on 2021/9/23.
//

#include "FedTree/FL/FLtrainer.h"
#include "FedTree/FL/FLparam.h"
#include "FedTree/Tree/GBDTparam.h"
#include "FedTree/FL/partition.h"
#include "FedTree/predictor.h"




extern "C" {

    void set_logger(int verbose) {
        if(verbose == 0) {
            el::Loggers::reconfigureAllLoggers(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            el::Loggers::reconfigureAllLoggers(el::Level::Trace, el::ConfigurationType::Enabled, "false");
            el::Loggers::reconfigureAllLoggers(el::Level::Info, el::ConfigurationType::Enabled, "false");
        }
        else if (verbose == 1) {
            el::Loggers::reconfigureAllLoggers(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            el::Loggers::reconfigureAllLoggers(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        }
    }

    void fit (int n_parties, int partition, float alpha, int n_hori, int n_verti, char *mode, char *partition_mode, char *privacy_tech, char *propose_split, char *merge_histogram, float variance, float privacy_budget,
              int depth, int n_trees, float min_child_weight, float lambda, float gamma, float column_sampling_rate, int verbose, int bagging, int n_parallel_trees, float learning_rate,
              char *objective, int num_class, int n_device, int max_num_bin,
              // DataSet info
              int row_size, float *val, int *row_ptr, int *col_ptr, float *label,
              // Tree info
              char *tree_method, Tree *&model, int *tree_per_iter, float *group_label,
              int *group, int num_group=0
              ) {
        LOG(INFO) << "Start training";

        // Initialize model params
        LOG(INFO) << "Initialize FL parameters";
        FLParam fl_param;
        LOG(INFO) << "n_parties: "  << n_parties;
        fl_param.n_parties = n_parties;
        LOG(INFO) << "partition:" << partition;
        fl_param.partition = partition == 1? true : false;
        LOG(INFO) << "alpha:" << alpha;
        fl_param.alpha = alpha;
        LOG(INFO) << "n_hori:" << n_hori;
        fl_param.n_hori = n_hori;
        LOG(INFO) << "n_verti:" << n_verti ;
        fl_param.n_verti = n_verti;
        LOG(INFO) << "mode:" << mode;
        fl_param.mode = mode;
        LOG(INFO) << "partition_mode" << mode;
        fl_param.partition_mode = mode;
        LOG(INFO) << "privacy_tech:" << privacy_tech;
        fl_param.privacy_tech = privacy_tech;
        LOG(INFO) << "propose_split:"  << propose_split;
        fl_param.propose_split = propose_split;
        LOG(INFO) << "merge_histogram:" << merge_histogram;
        fl_param.merge_histogram = merge_histogram;
        LOG(INFO) << "variance:" << variance;
        fl_param.variance = variance;
        LOG(INFO) << "privacy_budget:" << privacy_budget;
        fl_param.privacy_budget = privacy_budget;

        GBDTParam gbdt_param;
        LOG(INFO) << "depth:" << depth;
        gbdt_param.depth = depth;
        LOG(INFO) << "n_trees:" << n_trees;
        gbdt_param.n_trees = n_trees;
        LOG(INFO) << "min_child_weight:" << min_child_weight;
        gbdt_param.min_child_weight = min_child_weight;
        LOG(INFO) << "lambda:" << lambda;
        gbdt_param.lambda = lambda;
        LOG(INFO) << "gamma:" << gamma;
        gbdt_param.gamma = gamma;
        LOG(INFO) << "column_sampling_rate:" << column_sampling_rate;
        gbdt_param.column_sampling_rate = column_sampling_rate;
        LOG(INFO) << "verbose:" << verbose;
        gbdt_param.verbose = verbose;
        LOG(INFO) << "bagging:" << bagging;
        gbdt_param.bagging = bagging == 1? true : false;
        LOG(INFO) << "n_parallel_trees:" << n_parallel_trees;
        gbdt_param.n_parallel_trees = n_parallel_trees;
        LOG(INFO) << "learning_rate:" << learning_rate;
        gbdt_param.learning_rate = learning_rate;
        LOG(INFO) << "objective:" << objective;
        gbdt_param.objective = objective;
        LOG(INFO) << "num_class:" << num_class;
        gbdt_param.num_class = num_class;
        gbdt_param.tree_method = "hist";
        LOG(INFO) << "n_device:" << n_device;
        gbdt_param.n_device = n_device;
        gbdt_param.tree_per_rounds = 1;
        LOG(INFO) << "max_num_bin:" << max_num_bin;
        gbdt_param.max_num_bin = max_num_bin;
        gbdt_param.rt_eps = 1e-6;
        fl_param.gbdt_param = gbdt_param;

        set_logger(verbose);
        el::Loggers::reconfigureAllLoggers(el::ConfigurationType::PerformanceTracking, "false");

        LOG(INFO) << "Load Sparse Data to Training Set";
        DataSet training_set;
        training_set.load_from_sparse(row_size, val, row_ptr, col_ptr, label, group, num_group, gbdt_param);
        num_class = gbdt_param.num_class;

        LOG(INFO) << "Partition the Data";
        // Partition the dataset
        n_parties = fl_param.n_parties;
        vector<DataSet> subsets(n_parties);
        vector<SyncArray<bool>> feature_map(n_parties);
        std::map<int, vector<int>> batch_idxs;
        Partition partitioning;
        if (fl_param.partition == true) {
            if (fl_param.partition_mode == "hybrid") {
                if (fl_param.mode == "horizontal")
                    CHECK_EQ(fl_param.n_verti, 1);
                if (fl_param.mode == "vertical")
                    CHECK_EQ(fl_param.n_hori, 1);
                partitioning.horizontal_vertical_dir_partition(training_set, n_parties, fl_param.alpha, feature_map, subsets,
                                                            fl_param.n_hori, fl_param.n_verti);
            }
        }else if (fl_param.partition_mode == "vertical") {
            training_set.csr_to_csc();
            partitioning.homo_partition(training_set, n_parties, false, subsets, batch_idxs);
        }else if (fl_param.partition_mode == "horizontal") {
            partitioning.homo_partition(training_set, n_parties, true, subsets, batch_idxs);
        }
        // Update tree per rounds to match with number of class
        num_class = training_set.label.size();
        if (fl_param.gbdt_param.num_class != num_class) {
            LOG(INFO) << "updating number of classes from " << fl_param.gbdt_param.num_class << " to " << num_class;
            fl_param.gbdt_param.num_class = num_class;
        }
        fl_param.gbdt_param.tree_per_rounds = fl_param.gbdt_param.num_class;

        LOG(INFO) << "Initialize parties";
        // Initialize parties
        vector<Party> parties(n_parties);
        vector<int> n_instances_per_party(n_parties);
        for(int i = 0; i < n_parties; i++){
            parties[i].init(i, subsets[i], fl_param, feature_map[i]);
            n_instances_per_party[i] = subsets[i].n_instances();
        }

        // Initialize server
        LOG(INFO) << "initialize server";
        Server server;
        if (fl_param.mode == "vertical") {
            server.vertical_init(fl_param, training_set.n_instances(), n_instances_per_party, training_set.y, training_set.label);
        }else if (fl_param.mode == "horizontal") {
            server.horizontal_init(fl_param, training_set.n_instances(), n_instances_per_party, training_set);
        }else {
            server.init(fl_param, training_set.n_instances(), n_instances_per_party);
        }

        LOG(INFO) << "Run the trainer";
        // Run different training methods based on mode
        FLtrainer trainer;
        if(fl_param.mode == "hybrid") {
            trainer.hybrid_fl_trainer(parties, server, fl_param);
        } else if(fl_param.mode == "ensemble") {
            trainer.ensemble_trainer(parties, server, fl_param);
        }else if(fl_param.mode == "solo") {
            trainer.solo_trainer(parties, fl_param);
        }else if(fl_param.mode == "centralized") {
            GBDT gbdt;
            gbdt.train(fl_param.gbdt_param, training_set);
        }else if (fl_param.mode == "vertical") {
            trainer.vertical_fl_trainer(parties, server, fl_param);
        }else if (fl_param.mode == "horizontal") {
            trainer.horizontal_fl_trainer(parties, server, fl_param);
        }

        // Return boosted model
        vector<vector<Tree>> boosted_model = server.global_trees.trees;
        *tree_per_iter = (int)(boosted_model[0].size());
        model = new Tree[n_trees * (*tree_per_iter)];
        for(int i = 0; i < n_trees; i++)
        {
            for(int j = 0; j < *tree_per_iter; j++){
                model[i * (*tree_per_iter) + j] = boosted_model[i][j];
            }
        }
        for (int i = 0; i < training_set.label.size(); ++i) {
            group_label[i] = training_set.label[i];
        }
    }

void predict(int row_size, float *val, int *row_ptr, int *col_ptr, float *y_pred, Tree *&model,
                           int n_trees, int trees_per_iter, char *objective, int num_class, float learning_rate, float *group_label,
                           int *group, int num_group=0, int verbose=1) {
        //load model
        GBDTParam model_param;
        model_param.objective = objective;
        model_param.learning_rate = learning_rate;
        model_param.num_class = num_class;
        DataSet test_dataset;
        test_dataset.load_from_sparse(row_size, val, row_ptr, col_ptr, NULL, group, num_group, model_param);
        set_logger(verbose);
        test_dataset.label.clear();
        for (int i = 0; i < num_class; ++i) {
            test_dataset.label.emplace_back(group_label[i]);
        }
        // predict
        SyncArray<float_type> y_predict;
        vector<vector<Tree>> boosted_model_in_mem;
        for(int i = 0; i < n_trees; i++){
            boosted_model_in_mem.push_back(vector<Tree>());
            CHECK_NE(model, NULL) << "model is null!";
            for(int j = 0; j < trees_per_iter; j++) {
                boosted_model_in_mem[i].push_back(model[i * trees_per_iter + j]);
            }
        }
        GBDT gbdt_tree(boosted_model_in_mem);
        gbdt_tree.predict_raw(model_param, test_dataset, y_predict);
        //convert the aggregated values to labels, probabilities or ranking scores.
        std::unique_ptr<ObjectiveFunction> obj;
        obj.reset(ObjectiveFunction::create(model_param.objective));
        obj->configure(model_param, test_dataset);
        obj->predict_transform(y_predict);
        vector<float_type> y_pred_vec(y_predict.size());
        memcpy(y_pred_vec.data(), y_predict.host_data(), sizeof(float_type) * y_predict.size());
        for(int i = 0; i < y_pred_vec.size(); i++) {
            y_pred[i] = y_pred_vec[i];
        }//float_type may be double/float, so convert to float for both cases.
    }
}