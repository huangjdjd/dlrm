#include <torch/torch.h>

#include <iostream>

#include <torch/script.h>

#include <data2.h>

#include <vector>

#include <algorithm>

#include <omp.h>

#include <iomanip>

#include <fstream>
using namespace torch::indexing;

using namespace std;
/*
rmc1 
emb_vector_size = 32
emb_table = 32;//8
emb_size = 500000;// 2000000
lookupnum = 80

rmc2 
emb_vector_size = 64
emb_table = 32
emb_size = 500000
lookupnum = 120


rmc3
emb_vector_size = 32
emb_table = 25
emb_size = 800000
lookupnum = 20


*/
int emb_vector_size = 32;//64 32
int emb_table = 25;//32 8
int emb_size = 800000;//500000 4000000
int lookup_num = 20; //80  120



int dense_feature = 256;
int data_size = 800;//40 
int batch_size = 60;
int cache_size = 6*lookup_num*batch_size;
string s = "LRU_RMC2"; 
int epochs = 2;
double start_time;
std::ofstream myfile;
torch::nn::EmbeddingBag create_emb(cache2 &c, torch::Tensor emb_index, int table_ID){


    
    auto options = torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true);
    auto emb_index2 = emb_index.flatten();
    //cout<<emb_index2<<endl;
    vector<float> emb_id (emb_index2.data_ptr<float>(),emb_index2.data_ptr<float>() + emb_index2.numel());

    vector<int> emb(emb_id.begin(), emb_id.end());
    
    float ** emb_vec;

    if (c.write_table > 1){
        float ***emb_vec_slice;
        int count = 0;
        int size = 0;
        int last_table_ID = -1;
        int state;
        int l = 0;

        vector<int> emb_slice;
        vector<int> array_size;
        emb_vec_slice = new float **[emb.size()];
        for (auto it = emb.begin(); it != emb.end(); it++){

            state = table_ID;
            c.transfer_vector(*it,state);
            if (state != last_table_ID){
                if (emb_slice.size() == 0){

                }
                else{
                    emb_vec_slice[count++] = c.get_emb_vectors(emb_slice, last_table_ID);
                    size += emb_slice.size();
                    array_size.push_back(emb_slice.size());
                    vector<int>().swap(emb_slice);
                }
                
            }
            emb_slice.push_back(*it);
            last_table_ID = state;
        }
        emb_vec_slice[count++] = c.get_emb_vectors(emb_slice, last_table_ID);
        array_size.push_back(emb_slice.size());
        size += emb_slice.size();
 
        emb_vec = new float *[size]; 
        for (int i = 0; i < count; i++){
            for (int j = 0; j < array_size[i]; j++){
                emb_vec[l] = emb_vec_slice[i][j];
                l++;
            }

        }
        DATA e(emb_vec,batch_size*lookup_num,emb_vector_size);
        e.transfer_float1();


        auto weight = torch::from_blob( e.get_float1(),{batch_size*lookup_num,emb_vector_size},options);
        

    
        auto emb3 = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, emb_vector_size).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(0));
        auto emb2 = emb3.from_pretrained(weight,torch::nn::EmbeddingBagFromPretrainedOptions().freeze(false));
        
        return emb2;
    }
    else{

        emb_vec = c.get_emb_vectors(emb, table_ID);
    }

    DATA e(emb_vec,batch_size*lookup_num,emb_vector_size);
    e.transfer_float1();


    auto weight = torch::from_blob( e.get_float1(),{batch_size*lookup_num,emb_vector_size},options);
    

   
    auto emb3 = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, emb_vector_size).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(0));
    auto emb2 = emb3.from_pretrained(weight,torch::nn::EmbeddingBagFromPretrainedOptions().freeze(false));
    
    return emb2;
    


}

void save_emb(torch::Tensor update_parameter, torch::Tensor emb_index, cache2 &c, int table_ID){
    auto options = torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true);

    auto emb_index2 = emb_index.flatten();
    vector<float> emb_id (emb_index2.data_ptr<float>(),emb_index2.data_ptr<float>() + emb_index2.numel());
    vector<int> emb(emb_id.begin(), emb_id.end());
    //sort(emb.begin(), emb.end());
    if(c.write_table>1){
        
        for ( int y = 0 ;y < emb.size(); y++){
            int state = table_ID;
            c.transfer_vector(emb[y],state);
            if (c.get_vec()[state][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset] == NULL){
                c.get_vec()[state][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset] = new float [emb_vector_size];
            }
            for (int x = 0 ; x < emb_vector_size; x++){
               
                c.get_vec()[state][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset][x] = update_parameter[y][x].item<float>();
            }
        }
    }
    else{
        for ( int y = 0 ;y < emb.size(); y++){
            if (c.get_vec()[table_ID][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset] == NULL){
                c.get_vec()[table_ID][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset] = new float [emb_vector_size];
            }
            for (int x = 0 ; x < emb_vector_size; x++){
                c.get_vec()[table_ID][emb[y]/c.vector_page_offset].data[emb[y]%c.vector_page_offset][x] = update_parameter[y][x].item<float>();
            }
        }
    }
   



}


auto fc1_weight = torch::randn({128,dense_feature},torch::requires_grad());
auto fc2_weight = torch::randn({32,128},torch::requires_grad());
auto fc3_weight = torch::randn({32,32 + emb_table * emb_vector_size},torch::requires_grad());
auto fc4_weight = torch::randn({1,32},torch::requires_grad());

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
            break;
        case(4):
            fc->weight = fc4_weight;
            break;
    }
    
    return fc;
}



struct emb:torch::nn::Module
{
    emb(torch::Tensor x, cache2 &c){
        #pragma omp parallel for num_threads(4) ordered
        for (int i = 0; i < emb_table; i++){
 
            torch::Tensor emb_index = x.index({"...", Slice({dense_feature + i*lookup_num, dense_feature + (i+1) * lookup_num})});
            emb2  = create_emb(c,  emb_index, i);
            Embeddings->push_back(emb2);
            #pragma omp ordered
            {
                double time = omp_get_wtime();
                //cout<<setiosflags(ios::fixed)<<setprecision(4)<<time - start_time<<" ";
                myfile<<time - start_time<<";"<<i/c.get_pu()<<";"<<i%c.get_pu()<<";"<<c.write_buf[i].read_page_count<<endl;
                c.read_page(i);
            }

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
        for (int i = 0; i < 2; i++)
            lookup.slice(0, i,i+1) = torch::from_blob(lookupbag[i], {lookup_num}, options);



        auto lookupvec0 = Embeddings[0]->as<torch::nn::EmbeddingBag>()->forward(lookup);

        auto y = lookupvec0;
        for (int i = 1; i < emb_table; i++){
            auto lookupvec = Embeddings[i]->as<torch::nn::EmbeddingBag>()->forward(lookup);
            y = torch::cat({y,lookupvec},{1});
        }
        for(int i = 0; i < batch_size; ++i)
            delete [] lookupbag[i];
        delete [] lookupbag;

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

void  update_parameter(std::shared_ptr<emb> net2 ,torch::Tensor x, cache2 &c){
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
            //cout<<p.value()[0][0]<<endl;
        }
        if (p.key().compare(0,3,"fc1") == 0){

            cout<<torch::sum(p.value())<<endl;
            //cout<<p.value()[0][0]<<endl;
        }
        

    }

}

void  check_net2(std::shared_ptr<Net2> net2){
 
    for (auto &p: net2->named_parameters() ){

        //cout<<p.key()<<endl;

        if (p.key().compare(0,5,"fc1.w") == 0){

            cout<<torch::sum(p.value())<<endl;
            //cout<<p.value()[0][0]<<endl;
        }
        

    }

}

void reclaim_emb_cache(cache2 &c){
    cout<<"before reclaim vector = "<<c.lru_main[0].lru.size()<<endl;
    //#pragma omp parallel for num_threads(8) ordered
    for (int i = 0 ; i < c.get_table_num(); i++){
        //#pragma omp critical
        //{
        while (c.lru_main[i].lru.size() > cache_size){
            c.Reclaim_vector_page(i);
        }
        //}
        //#pragma omp ordered
        //{
            double time = omp_get_wtime();
            //cout<<setiosflags(ios::fixed)<<setprecision(4)<<time - start_time<<" ";
            myfile<<time - start_time<<";"<<i/c.get_pu()<<";"<<i%c.get_pu()<<";"<<";"<<c.write_buf[i].write_page_count<<endl;
            c.write_page(i);
        //}

    }
}
    

int main(int argc, char *argv[])
{
    // Create a new Net.


	std::string loc_states, loc_labels;
    loc_states = "1";
    //loc_labels = {"1"};
   double program_start_time=omp_get_wtime(); 
    torch::Tensor test,train,train2;
    //auto net = std::make_shared<Net>();
    

    test = torch::tensor({{0},{1}});
    
    //cout<<data_set.get();
    // Generate a data loader.
    
    if (argc == 4){
	    int v1 = stoi(argv[1]);
	    int v2 = stoi(argv[2]);
	    int v3 = stoi(argv[3]);
	    
	    data_size = v1;
	    batch_size = v2;
	    cache_size = v3*lookup_num*batch_size;
	    
    }
    


    string s2 = std::to_string(cache_size/(lookup_num*batch_size));
    string s3 = std::to_string(batch_size);
	string s4 = std::to_string(data_size);
	s = s + "_C" + s2 + "_B" + s3 + "_D" + s4 + ".csv";
    myfile.open(s);
    myfile<<"'Time';'Channel';'PU';'Read';'Write'"<<endl;
	cout <<"run  "<<s<<endl;
   // start_time = omp_get_wtime();
    

    auto net2 = std::make_shared<Net2>();
    cache2 emb_cache(emb_table ,emb_size,emb_vector_size);
    
    auto data_set = MyDataset(loc_states).map(torch::data::transforms::Stack<>());
    auto data_loader = torch::data::make_data_loader<torch::data::samplers::SequentialSampler>(
        std::move(data_set), 
        batch_size);
    try {
    emb_cache.write();
} catch (const std::exception& e) {
    std::cerr << "寫入異常: " << e.what() << std::endl;
}
  emb_cache.write(); // adjust 
//    emb_cache.pa_init();
    emb_cache.hot_page_count = data_size/4;
    int bb = batch_size;
    start_time = omp_get_wtime();
    for (size_t epoch = 1; epoch <= epochs; ++epoch){

        

        int b = 0;
        for (auto& batch : *data_loader) {  

            cout << "Batch: " << b<<"    ";
            cout<<"epoch = "<<epoch<<endl;
            auto data = batch.data;

            batch_size = bb;
            if(b == data_size/batch_size && data_size%batch_size != 0){
                batch_size = data_size%batch_size;
            }

            auto embedding = std::make_shared<emb>(data,emb_cache);
            reclaim_emb_cache(emb_cache);
            vector<torch::Tensor> node;

            node.emplace_back(embedding->parameters()[0]);
            node.emplace_back(net2->parameters()[0]);

            torch::optim::SGD optimizer(
                node, torch::optim::SGDOptions(0.01).momentum(0.5));
            

            

            

            optimizer.zero_grad();
            torch::Tensor lookup = embedding->forward(data);
            // cout<<lookup<<endl;

            
            torch::Tensor prediction = net2->forward(data, lookup);

            //check_net(embedding);

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

            cout<<"avector = "<<emb_cache.lru_main[0].lru.size()<<endl;
           //check_net(embedding);

            // node.pop_back();
            // node.pop_back();
            b++;
        }
    }
   double program_end_time=omp_get_wtime();
    double total_execute_time=program_end_time-program_start_time;
    cout<<endl;
    cout<<"total execute time: "<<total_execute_time<<" seconds"<<endl;
    cout<<"train time: "<<(program_end_time-start_time)<<" seconds"<<endl;
 
string chunk_stats_filename = "chunk_statistics_rmc3" + s2 + "_B" + s3 + "_D" + s4 + ".csv";
emb_cache.dump_chunk_sector_stats(chunk_stats_filename); 
// emb_cache.write_chunk_statistics_to_csv(chunk_stats_filename);   
    

    // Instantiate an SGD optimization algorithm to update our Net's parameters.

    
    
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
