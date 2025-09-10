#include <torch/torch.h>
#include <iostream>

#include <torch/script.h>

using namespace torch::indexing;

using namespace std;

int emb_vector_size = 32;
int emb_table = 2;
int emb_size = 100000;
int batch_size = 2;
int lookup_num = 4;
int dense_feature = 256;
int key = 0;
int emb_num = 1;
int data_size = 4;

torch::nn::EmbeddingBag create_emb(torch::Tensor emb_index){


    
        
   

    //cout<<emb_index<<endl;
    int temp_emb1[2][4];
    

    
    /*
    for (int i = 0; i < batch_size; i++){
        for (int  j = 0; j < lookup_num; j++ ){
            temp_emb1[i][j] = emb_index[i][j].item<int>();
        }
    }
    */
    
    torch::manual_seed(0);
    auto lookup_weight = torch::randn({batch_size*lookup_num,emb_vector_size},torch::requires_grad());
    //cout<<lookup_weight<<endl;

        
    auto emb = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, emb_vector_size).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(0));
    auto emb2 = emb.from_pretrained(lookup_weight,torch::nn::EmbeddingBagFromPretrainedOptions().freeze(false));
    
    return emb2;
    


}

void save_emb(torch::nn::EmbeddingBag emb, torch::Tensor emb_index){


    
        
   

    //cout<<emb_index<<endl;

    

    
    /*
    for (int i = 0; i < batch_size; i++){
        for (int  j = 0; j < lookup_num; j++ ){
            temp_emb1[i][j] = emb_index[i][j].item<int>();
        }
    }
    */
    



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


void save_fc(torch::nn::Linear fc , int ff){


    switch(ff){
        case(1):
            fc1_weight = fc->weight;
            break;
        case(2):
            fc2_weight = fc->weight;
            break;
        case(3):
            fc3_weight = fc->weight;
            break;
        case(4):
            fc4_weight = fc->weight;
            break;
    }
    
}


struct Net2 : torch::nn::Module
{
    Net2(torch::Tensor x)
    {
        torch::Tensor emb_index0 = x.index({"...", Slice({dense_feature, dense_feature + lookup_num})});

        
        emb0 = create_emb(emb_index0);
        emb0 = register_module("emb0", emb0);
        

        torch::Tensor emb_index1 = x.index({"...", Slice({dense_feature + lookup_num, dense_feature + 2 * lookup_num})});
        emb1 = create_emb(emb_index1);
        emb1 = register_module("emb1", emb1);


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



    }

    torch::Tensor forward(torch::Tensor x)
    {   
   
        //auto emb = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, 32).max_norm(2).norm_type(2.5).scale_grad_by_freq(false).sparse(true).mode(torch::kSum).padding_idx(1));
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


        torch::Tensor nn_data = x.index({"...", Slice({None, dense_feature})});
        //cout<<nn_data<<endl;
        
        //emb lookup here
        //torch::nn::EmbeddingBag emb1 = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(batch_size*lookup_num, 32).max_norm(2).norm_type(2.5).scale_grad_by_freq(false).sparse(true).mode(torch::kSum).padding_idx(1));
        
        torch::Tensor emb_index0 = x.index({"...", Slice({dense_feature,dense_feature + lookup_num})});
        torch::Tensor emb_index1 = x.index({"...", Slice({dense_feature + lookup_num, dense_feature + 2 * lookup_num})});
        //emb = modify_emb(emb,emb_index);

        
        //save previous network parameter
        if (key == 1 ){
            save_emb(emb0, emb_index0);
            save_emb(emb1, emb_index1);
            save_fc(fc1, 1);
            save_fc(fc2, 2);
            save_fc(fc3, 3);
            save_fc(fc4, 4);
        }
        
        //cout<<emb->weight<<endl;
        auto lookupvec0 = emb0->forward(lookup);
        auto lookupvec1 = emb1->forward(lookup);
        cout<<torch::_shape_as_tensor(lookupvec1)<<endl;
        //emb->weight.requires_grad()="true";

        //--------------bot Dense forward -------------------
        x = torch::relu(fc1->forward(nn_data));
        x = torch::relu(fc2->forward(x));
        //cout<<x<<endl;
        //--------------bot Dense forward -------------------       

        x= torch::cat({x,lookupvec0},{1});
        x= torch::cat({x,lookupvec1},{1});
        
        x = torch::relu(fc3->forward(x));
        //cout<<x<<endl;
        x = torch::relu(fc4->forward(x));
        return x;
    }


    torch::nn::Linear fc1{nullptr}, fc2{nullptr}, fc3{nullptr}, fc4{nullptr};
    torch::nn::EmbeddingBag emb0{nullptr}, emb1{nullptr};
 
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
            train = torch::randn({data_size*batch_size,dense_feature});
            //train = torch::tensor({{0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0},{0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0}});
            //train2 = torch::tensor({{0.0,1.0,2.0,3.0},{0.0,1.0,2.0,3.0}});
            train2 = torch::randint(emb_size, {data_size*batch_size, emb_table*lookup_num});
            //cout<<"emb index = "<<train2<<endl;
            train = torch::cat({train,train2},{1});
            //cout<<train2<<endl;
            //test = torch::tensor({{0.1656},{0.849}});
            //cout<<test<<endl;
            cout<<torch::_shape_as_tensor(train)<<endl;
            test = torch::randn({data_size*batch_size,1});
            //cout<<test<<endl;
        };
        
        torch::data::Example<> get(size_t index) override{
        // You may for example also read in a .csv file that stores locations
        // to your data and then read in the data at this step. Be creative.
        
        torch::Tensor t1 = train[index];
        torch::Tensor t2 = test[index];
        //cout<<index<<endl;
        cout<<torch::_shape_as_tensor(train[index])<<endl;
        //index ++ ;
        
        
        return {t1, t2};
    
        };
        torch::optional<size_t>size()const override {
		
		    return data_size;
	    };

    
};







int main()
{
    // Create a new Net.

	std::string loc_states, loc_labels;
    loc_states = "1";
    //loc_labels = {"1"};
    
    torch::Tensor test,train,train2;
    //auto net = std::make_shared<Net>();
    

    test = torch::tensor({{0},{1}});
    auto data_set = MyDataset(loc_states).map(torch::data::transforms::Stack<>());
    //cout<<data_set.get();
    // Generate a data loader.
    
    auto data_loader = torch::data::make_data_loader<torch::data::samplers::SequentialSampler>(
        std::move(data_set), 
        batch_size);
    
   

    for (size_t epoch = 1; epoch <= 10; ++epoch){


        size_t index=0;

        for (auto& batch : *data_loader) {  
            cout << "Batch size: " << batch.data.size(0) << " | Labels: ";
            cout<<"epoch = "<<epoch<<endl;
            auto data = batch.data;
            
            //cout<<data_loader<<endl;
            auto net2 = std::make_shared<Net2>(data);
            torch::optim::SGD optimizer(
                net2->parameters(), torch::optim::SGDOptions(0.01).momentum(0.5));


            
            
            //cout<<data<<endl;

            torch::Tensor prediction = net2->forward(data);
            
            //cout<<prediction<<endl;
            //cout<<batch.target<<endl;
            //auto labels = torch::argmax(batch.target, 1);
            //cout<<labels<<endl;
            //cout<<labels<<endl;
            //torch::Tensor loss = torch::nll_loss(prediction, labels);
            torch::Tensor loss = torch::mse_loss(prediction, batch.target);
            /*
            cout<<"loss"<<endl;
            cout<<loss<<endl;
            cout<<"loss"<<endl;
            */
            //cout<<net2->parameters()<<endl;
            
            loss.backward();
            
            optimizer.step();

            key = 1;
            if (++index%2 == 0 ){
                //cout<<"test"<<endl;
            }
        }
    }


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