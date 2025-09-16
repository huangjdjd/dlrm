#include <torch/torch.h>

#include <iostream>
#include <fstream>
#include <torch/script.h>
#include <fstream>
#include <data.h>
#include <iomanip>
#include <vector>
#include <omp.h>
#include <algorithm>
using namespace torch::indexing;

using namespace std;
/*
rmc1 
emb_vector_size = 32
emb_table = 32; //8
emb_size = 500000 ;//2000000  
lookupnum = 80

rmc2 
emb_vector_size = 64
emb_table = 32
emb_size = 500000
lookupnum = 120


rmc3
emb_vector_size = 32
emb_table = 40;//10
emb_size = 500000;//2000000
lookupnum = 20


*/
int emb_vector_size = 32;//64
int emb_table = 32;//32
int emb_size = 500000;//500000
int lookup_num = 80;



int dense_feature = 256;
int batch_size = 60;//4
int data_size = 800;//40
int cache_size = 6*emb_table*lookup_num*batch_size;
string s = "LRU_BASE_RMC1"; 
int epochs = 2;
double start_time;
std::ofstream myfile;

torch::nn::EmbeddingBag create_emb(cache &c, torch::Tensor emb_index, int table_ID){


    
    auto options = torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true);
    auto emb_index2 = emb_index.flatten();
    //cout<<emb_index2<<endl;
    vector<float> emb_id (emb_index2.data_ptr<float>(),emb_index2.data_ptr<float>() + emb_index2.numel());

    vector<int> emb(emb_id.begin(), emb_id.end());
    sort(emb.begin(), emb.end());
    float ** emb_vec;

    emb_vec = c.get_emb_vectors(emb, table_ID);

    DATA e(emb_vec,batch_size*lookup_num,emb_vector_size);
    e.transfer_float1();


    auto weight = torch::from_blob( e.get_float1(),{batch_size*lookup_num,emb_vector_size},options);
    

   
    //cout<<weight<<endl;

    //auto weight = torch::randn({batch_size*lookup_num,emb_vector_size},torch::requires_grad());
    auto emb3 = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, emb_vector_size).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(0));
    auto emb2 = emb3.from_pretrained(weight,torch::nn::EmbeddingBagFromPretrainedOptions().freeze(false));
    
    return emb2;
    


}

void save_emb(torch::Tensor update_parameter, torch::Tensor emb_index, cache &c, int table_ID){
    auto options = torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true);

    auto emb_index2 = emb_index.flatten();
    vector<float> emb_id (emb_index2.data_ptr<float>(),emb_index2.data_ptr<float>() + emb_index2.numel());
    vector<int> emb(emb_id.begin(), emb_id.end());
    sort(emb.begin(), emb.end());

    for ( int y = 0 ;y < emb.size(); y++){
        for (int x = 0 ; x < emb_vector_size; x++){
            c.get_vec()[table_ID][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset][x] = update_parameter[y][x].item<float>();
        }
	//cout<<"vector_page_offset"<<c.vector_page_offset<<endl;
    }
   
   



}


auto fc1_weight = torch::randn({128,dense_feature},torch::requires_grad());
auto fc2_weight = torch::randn({32,128},torch::requires_grad());
auto fc3_weight = torch::randn({32,32 + emb_table * emb_vector_size},torch::requires_grad());
auto fc4_weight = torch::randn({1,32},torch::requires_grad());
/*
torch::Tensor fc1_weight;
torch::Tensor fc2_weight;
torch::Tensor fc3_weight;
torch::Tensor fc4_weight;*/
torch::nn::Linear create_fc(int input, int output , int ff){
    torch::nn::Linear fc = torch::nn::Linear(input, output);
     
    switch(ff){
        case(1):
            fc->weight = fc1_weight;
            break;
        case(2):
            fc->weight = fc2_weight;
            break;
        case(3):
            fc->weight = fc3_weight;
	  // fc->weight.data() = torch::randn_like(fc->weight.data());
           break;
        case(4):
            fc->weight = fc4_weight;
            break;
    }
    
    return fc;
    //return torch::nn::Linear(input, output);
}



struct emb:torch::nn::Module
{
    emb(torch::Tensor x, cache &c){
//	 #pragma omp parallel for num_threads(4) ordered
        for (int i = 0; i < emb_table; i++){
 
            torch::Tensor emb_index = x.index({"...", Slice({dense_feature + i*lookup_num, dense_feature + (i+1) * lookup_num})});
            emb2  = create_emb(c, emb_index, i);
            Embeddings->push_back(emb2);
      /*   #pragma omp critical
        {
            torch::nn::EmbeddingBag emb_local = create_emb(c, emb_index, i);
            Embeddings->push_back(emb_local);
        }*/
            
            //cout<<setiosflags(ios::fixed)<<setprecision(4)<<time - start_time<<" ";

        }
		for (int i = 0; i < emb_table; i++){
			double time = omp_get_wtime();
			myfile<<time - start_time<<";"<<i/c.get_pu()<<";"<<i%c.get_pu()<<";"<<c.write_buf[i].read_page_count<<endl;

            c.read_page(i);
        }
        Embeddings = register_module("Embeddings", Embeddings);
    }
    torch::Tensor forward(torch::Tensor x){
        auto options = torch::TensorOptions().dtype(torch::kInt32);
        int** lookupbag = new int*[batch_size];
        for(int i = 0; i < batch_size; ++i)
            lookupbag[i] = new int[lookup_num];

        //cout<<emb->weight<<endl;
        int k = 0;
        for (int i = 0; i < batch_size; i++){
            for (int j = 0; j < lookup_num; j++){
                lookupbag[i][j] = k;
                k++;
            }
        }
        
        auto lookup = torch::zeros({batch_size,lookup_num}, options);
        for (int i = 0; i < batch_size; i++)
            lookup.slice(0, i,i+1) = torch::from_blob(lookupbag[i], {lookup_num}, options);



        auto lookupvec0 = Embeddings[0]->as<torch::nn::EmbeddingBag>()->forward(lookup);

        auto y = lookupvec0;
        for (int i = 1; i < emb_table; i++){
            auto lookupvec = Embeddings[i]->as<torch::nn::EmbeddingBag>()->forward(lookup);
            y = torch::cat({y,lookupvec},{1});
        }

       //  for(int i = 0; i < batch_size; ++i){
         //   delete [] lookupbag[i];}
        //delete [] lookupbag;

        return y;


    }

    torch::nn::ModuleList Embeddings = torch::nn::ModuleList();
    torch::nn::EmbeddingBag emb2{nullptr};
};






struct Net2 : torch::nn::Module
{
    Net2()
    {
        

        
  
        

        //crate embedding table size base on batch to handle multi of data.
        // for (int i = 0; i < emb_table; i++){
        //     emb =  torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, emb_vector_size).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(0));
        //     Embeddings->push_back(emb);
        // }


        fc1 = create_fc(dense_feature, 128 ,1);
        //fc1 = torch::nn::Linear(dense_feature, 128);

        fc2 = create_fc(128, 32 ,2);
        //fc2 = torch::nn::Linear(128, 32);
        fc3 = create_fc(32 + emb_table * emb_vector_size, 32 ,3);
        //fc3 = torch::nn::Linear(64, 32);
   
        fc4 = create_fc(32, 1 ,4);
        //fc4 = torch::nn::Linear(32, 1);

        fc1 = register_module("fc1", fc1);
        fc2 = register_module("fc2", fc2);
        fc3 = register_module("fc3", fc3);
        fc4 = register_module("fc4", fc4);
       // Embeddings = register_module("Embeddings", Embeddings);


    }

    torch::Tensor forward(torch::Tensor x, torch::Tensor y)
    {   


        torch::Tensor nn_data = x.index({"...", Slice({None, dense_feature})});
        //--------------bot Dense forward -------------------
        x = torch::relu(fc1->forward(nn_data));
        x = torch::relu(fc2->forward(x));
        //cout<<x<<endl;
        //--------------bot Dense forward -------------------       
        
        x= torch::cat({x,y},{1});
      
        
        x = torch::relu(fc3->forward(x));
        //cout<<x<<endl;
        x = torch::relu(fc4->forward(x));
        return x;
    }


    torch::nn::Linear fc1{nullptr}, fc2{nullptr}, fc3{nullptr}, fc4{nullptr};

    //torch::nn::ModuleList Embeddings = torch::nn::ModuleList();
};












torch::Tensor read_data(const std::string& loc)
{
    torch::Tensor tensor = torch::tensor({0,1,2,3,4,5,6,7,8,9});

    // Here you need to get your data.

    return tensor;
};
class MyDataset: public torch::data::Dataset<MyDataset>
{
    private:
        std::string  states_;
        torch::Tensor test,train,train2;
    public:
        
        MyDataset(std::string loc_states){
            torch::manual_seed(0);

            train = torch::randn({data_size*batch_size,dense_feature});

            train2 = torch::randint(0,emb_size, {data_size*batch_size, emb_table*lookup_num});

            train = torch::cat({train,train2},{1});

            test = torch::randn({data_size*batch_size,1});

        };
        
        torch::data::Example<> get(size_t index) override{

            torch::Tensor t1 = train[index];
            torch::Tensor t2 = test[index];

        
            return {t1, t2};
    
        };
        torch::optional<size_t>size()const override {
		
		    return data_size;
	    };

    
};

void  update_parameter(std::shared_ptr<emb> net2 ,torch::Tensor x, cache &c){
    int i = 0;
    //torch::NoGradGuard no_grad;
    for (auto &p: net2->named_parameters() ){

       
        if (p.key().compare(0,3,"Emb") == 0){

            torch::Tensor emb_index = x.index({"...", Slice({dense_feature + i*lookup_num, dense_feature + (i+1) * lookup_num})});
            auto z = p.value();
            save_emb(z,emb_index,c,i);
            //auto k = create_emb(c,emb_index, i);

            //cout<<p.value()[0][0]<<endl;
            i++;
        }
        
    }

}



void  check_net(std::shared_ptr<emb> net2){
 
    for (auto &p: net2->named_parameters() ){

        cout<<p.key()<<endl;
        if (p.key().compare(0,3,"Emb") == 0){

            cout<<torch::sum(p.value())<<endl;
            cout<<p.value()[0][0]<<endl;
        }
        if (p.key().compare(0,3,"fc1") == 0){

            cout<<torch::sum(p.value())<<endl;
            cout<<p.value()[0][0]<<endl;
        }
        

    }

}

void  check_net2(std::shared_ptr<Net2> net2){
 
    for (auto &p: net2->named_parameters() ){

        //cout<<p.key()<<endl;

        if (p.key().compare(0,5,"fc1.w") == 0){

            //cout<<torch::sum(p.value())<<endl;
            //cout<<p.value()[0][0]<<endl;
        }
        

    }

}

void reclaim_emb_cache(cache &c){
    cout<<"before reclaim page = "<<c.page_count<<endl;
    while (c.page_count > cache_size){
        for (int i = 0 ; i < emb_table; i++){
            c.Reclaim_vector_page(i);
        }
    }
	for (int i = 0 ; i < emb_table; i++){


            double time = omp_get_wtime();
            //cout<<setiosflags(ios::fixed)<<setprecision(4)<<time - start_time<<" ";
			myfile<<time - start_time<<";"<<i/c.get_pu()<<";"<<i%c.get_pu()<<";"<<";"<<c.write_buf[i].write_page_count<<endl;
            c.write_page(i);
    }
}


int main(int argc, char *argv[])
{
    // Create a new Net.
  // omp_set_num_threads(4);	 
	std::string loc_states, loc_labels;
    loc_states = "1";
    //loc_labels = {"1"};
   double program_start_time = omp_get_wtime(); 
    torch::Tensor test,train,train2;
    //auto net = std::make_shared<Net>();
    

    test = torch::tensor({{0},{1}});
	int c;
    if (argc == 4){
	    int v1 = stoi(argv[1]);
	    int v2 = stoi(argv[2]);
	    int v3 = stoi(argv[3]);
	    
	    data_size = v1;
	    batch_size = v2;
	    cache_size = v3*emb_table*lookup_num*batch_size;
	    c = v3;
    }
	else{
		c = 6;	
	}

    string s2 = std::to_string(cache_size/(emb_table*lookup_num*batch_size));
    string s3 = std::to_string(batch_size);
	string s4 = std::to_string(data_size);
	s = s + "_C" + s2 + "_B" + s3 + "_D" + s4 + ".csv";
    myfile.open(s);
    myfile<<"'Time';'Channel';'PU';'Read';'Write'"<<endl;
	cout <<"run  "<<s<<endl;
//    start_time = omp_get_wtime();
	
    auto data_set = MyDataset(loc_states).map(torch::data::transforms::Stack<>());
    //cout<<data_set.get();
    // Generate a data loader.
    //    auto net2 = std::make_shared<Net2>();
   // cache emb_cache(emb_table,emb_size,emb_vector_size);
    auto data_loader = torch::data::make_data_loader<torch::data::samplers::SequentialSampler>(
        std::move(data_set), 
        batch_size);
 
    auto net2 = std::make_shared<Net2>();
    
    cache emb_cache(emb_table,emb_size,emb_vector_size);
    emb_cache.write();
//    emb_cache.pa_init();
    int bb = batch_size;
     start_time = omp_get_wtime();
    for (size_t epoch = 1; epoch <= epochs; ++epoch){


       cout<<"test"<<endl;
        int b = 0;
        for (auto& batch : *data_loader) {  
            cout<<"test2"<<endl;
            cout << "Batch: " << b<<"    ";
            cout<<"epoch = "<<epoch<<endl;
            auto data = batch.data;
            
            batch_size = bb;
            if(b == data_size/batch_size && data_size%batch_size != 0){
                batch_size = data_size%batch_size;
            }
             //        cout<<"embedding"<<endl;
         auto embedding = std::make_shared<emb>(data,emb_cache);
	                //cout<<"embedding_2"<<endl;
           // cout<<"reclaim"<<endl;
	 /*          std::shared_ptr<emb> embedding;
        try {
            embedding = std::make_shared<emb>(data, emb_cache);
        } catch (const std::exception& e) {
            std::cerr << "Error creating embedding: " << e.what() << std::endl;
            continue;
        }*/
            reclaim_emb_cache(emb_cache);
	       //     cout<<"reclaim end"<<endl;
			if (b+1 == c && epoch==1){
				cache_size = emb_cache.page_count;
			}
            vector<torch::Tensor> node;


            node.emplace_back(embedding->parameters()[0]);
            node.emplace_back(net2->parameters()[0]);

            torch::optim::SGD optimizer(
                node, torch::optim::SGDOptions(0.01).momentum(0.5));
            

            

            

            optimizer.zero_grad();
            torch::Tensor lookup = embedding->forward(data);
            // cout<<lookup<<endl;

            
            torch::Tensor prediction = net2->forward(data, lookup);

  //          check_net(embedding);

            torch::Tensor loss = torch::mse_loss(prediction, batch.target);
            //cout<<"loss="<<loss<<endl;
            /*
            cout<<"loss"<<endl;
            cout<<loss<<endl;
            cout<<"loss"<<endl;
            */
            //cout<<net2->parameters()<<endl;
            
            loss.backward();
            cout<<"backward"<<endl;
            optimizer.step();

            update_parameter(embedding, data, emb_cache);

            cout<<"page = "<<emb_cache.page_count<<endl;
//           check_net(embedding);

            // node.pop_back();
            // node.pop_back();
	    cout<<"b++"<<endl;
            b++;
	    cout<<"b++2"<<endl;
        }
    }
   
    double program_end_time=omp_get_wtime();
    double total_execute_time=program_end_time-program_start_time;

    cout<<endl;
    cout<<"total execution time: "<<total_execute_time<<" seconds"<<endl;
    cout<<"training time: "<<(program_end_time-start_time)<<" seconds"<<endl;
    cout<<"set up time: "<<(start_time-program_start_time)<<" seconds"<<endl;
   string chunk_stats_filename = "chunk_statistics_rmc1" + s;
    emb_cache.dump_chunk_sector_stats(chunk_stats_filename);
    
    // Instantiate an SGD optimization algorithm to update our Net's parameters.

//    string chunk_stats_filename = "base_rmc1_chunk_statistics_" + s2 + "_B" + s3 + "_D" + s4 + ".csv";
  //  emb_cache.write_chunk_statistics_to_csv(chunk_stats_filename);    
    
    /*
    for (size_t epoch = 1; epoch <= 10; ++epoch)
    {
        size_t batch_index = 0;
        // Iterate the data loader to yield batches from the dataset.
        for (auto &batch : *data_loader)
        {
            // Reset gradients.
            optimizer.zero_grad();
            // Execute the model on the input data.
            torch::Tensor prediction = net->forward(batch.data);
            // Compute a loss value to judge the prediction of our model.
            torch::Tensor loss = torch::nll_loss(prediction, batch.target);
            // Compute gradients of the loss w.r.t. the parameters of our model.
            loss.backward();
            // Update the parameters based on the calculated gradients.
            optimizer.step();
            // Output the loss and checkpoint every 100 batches.
            if (++batch_index % 10 == 0)
            {
                std::cout << "Epoch: " << epoch << " | Batch: " << batch_index
                          << " | Loss: " << loss.item<float>() << std::endl;
                // Serialize your model periodically as a checkpoint.
                torch::save(net, "net.pt");
            }
        }
    }
    */
}
