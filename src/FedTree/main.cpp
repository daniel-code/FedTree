//
// Created by liqinbin on 10/13/20.
//

#include "FedTree/FL/FLparam.h"
#include "FedTree/FL/FLtrainer.h"
#include "FedTree/FL/partition.h"
#include "FedTree/parser.h"
#include "FedTree/dataset.h"
#include "FedTree/Tree/gbdt.h"


#ifdef _WIN32
INITIALIZE_EASYLOGGINGPP
#endif
int main(int argc, char** argv){
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Format, "%datetime %level %fbase:%line : %msg");
    el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
    el::Loggers::addFlag(el::LoggingFlag::FixedTimeFormat);

/*

    //initialize parameters
    FLParam fl_param;
    Parser parser;
    parser.parse_param(fl_param, argc, argv);

    //load dataset from file/files
    DataSet dataset;
    dataset.load_from_file(fl_param.dataset_path);

    //initialize parties and server *with the dataset*
    vector<Party> parties;
    for(i = 0; i < fl_param.n_parties; i++){
        Party party;
        parties.push_back(party);
    }
    Server server;

    //train
    FLtrainer trainer;
    model = trainer.train(parties, server, fl_param);

    //test
    Dataset test_dataset;
    test_dataset.load_from_file(fl_param.test_dataset_path);
    acc = model.predict(test_dataset);
*/

//centralized training test
    FLParam fl_param;
    Parser parser;
    parser.parse_param(fl_param, argc, argv);
    GBDTParam &model_param = fl_param.gbdt_param;
    if(model_param.verbose == 0) {
        el::Loggers::reconfigureAllLoggers(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        el::Loggers::reconfigureAllLoggers(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        el::Loggers::reconfigureAllLoggers(el::Level::Info, el::ConfigurationType::Enabled, "false");
    }
    else if (model_param.verbose == 1) {
        el::Loggers::reconfigureAllLoggers(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        el::Loggers::reconfigureAllLoggers(el::Level::Trace, el::ConfigurationType::Enabled, "false");
    }

    if (!model_param.profiling) {
        el::Loggers::reconfigureAllLoggers(el::ConfigurationType::PerformanceTracking, "false");
    }
    if(fl_param.mode == "centralized") {
        DataSet dataset;
        vector <vector<Tree>> boosted_model;
        dataset.load_from_file(model_param.path, fl_param);
        GBDT gbdt;
        gbdt.train(model_param, dataset);
        parser.save_model("tgbm.model", model_param, gbdt.trees, dataset);
    }
    else{
        int n_parties = fl_param.n_parties;
        vector<DataSet> subsets(n_parties);
        if(fl_param.partition == true){
            DataSet dataset;
            dataset.load_from_file(model_param.path, fl_param);
            Partition partition;
            if(fl_param.mode == "hybrid"){
                vector<float> alpha(n_parties, fl_param.alpha);
                partition.hybrid_partition(dataset, n_parties, alpha, subsets);
            }
            else{
                std::cout<<"not supported yet"<<std::endl;
                exit(1);
            }
        }
        vector<Party> parties(n_parties);
        for(int i = 0; i < n_parties; i++){
            parties[i].init(i, subsets[i], fl_param);
        }
        Server server;
        FLtrainer trainer;
        if(fl_param.mode == "hybrid"){
            trainer.hybrid_fl_trainer(parties, server, fl_param);
        }
//        parser.save_model("global_model", fl_param.gbdt_param, server.global_trees.trees, dataset);
    }
    return 0;
}
