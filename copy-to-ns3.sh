# Configure the following values
NS3_FOLDER=~/workspace/ns-allinone-3.25/ns-3.25
PROJECT_FOLDER=~/workspace/2018_Blockchain_ETRI_ns3

# Do not change
cp $PROJECT_FOLDER/applications/model/blockchain.h $NS3_FOLDER/src/applications/model/ 
cp $PROJECT_FOLDER/applications/model/blockchain.cc $NS3_FOLDER/src/applications/model/
cp $PROJECT_FOLDER/applications/model/blockchain-miner.h $NS3_FOLDER/src/applications/model/
cp $PROJECT_FOLDER/applications/model/blockchain-miner.cc $NS3_FOLDER/src/applications/model/
cp $PROJECT_FOLDER/applications/model/blockchain-node.h $NS3_FOLDER/src/applications/model/
cp $PROJECT_FOLDER/applications/model/blockchain-node.cc $NS3_FOLDER/src/applications/model/
cp $PROJECT_FOLDER/applications/helper/blockchain-miner-helper.h $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/applications/helper/blockchain-miner-helper.cc $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/applications/helper/blockchain-node-helper.h $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/applications/helper/blockchain-node-helper.cc $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/applications/helper/blockchain-topology-helper.h $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/applications/helper/blockchain-topology-helper.cc $NS3_FOLDER/src/applications/helper/
cp $PROJECT_FOLDER/internet/helper/ipv4-address-helper-custom.h $NS3_FOLDER/src/internet/helper/
cp $PROJECT_FOLDER/internet/helper/ipv4-address-helper-custom.cc $NS3_FOLDER/src/internet/helper/

