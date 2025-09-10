#include <torch/torch.h>
#include <iostream>
using namespace std;
struct Net : torch::nn::Module
{
    Net()
    {
        // Construct and register two Linear submodules.
        fc1 = register_module("fc1", torch::nn::Linear(784, 64));
        fc2 = register_module("fc2", torch::nn::Linear(64, 32));
        fc3 = register_module("fc3", torch::nn::Linear(32, 10));
    }

    // Implement the Net's algorithm.
    torch::Tensor forward(torch::Tensor x)
    {
        // Use one of many tensor manipulation functions.
        x = torch::relu(fc1->forward(x.reshape({x.size(0), 784})));
        x = torch::dropout(x, /*p=*/0.5, /*train=*/is_training());
        x = torch::relu(fc2->forward(x));
        x = torch::log_softmax(fc3->forward(x), /*dim=*/1);
        return x;
    }

    // Use one of many "standard library" modules.
    torch::nn::Linear fc1{nullptr}, fc2{nullptr}, fc3{nullptr};
};
int main(){
    double array[] = {1, 2, 3};
    double offset[] = {0, 0, 2};
    //auto options2 = torch::TensorOptions().dtype(torch::kSum);

    auto emb = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(8, 4).max_norm(2).norm_type(2.5).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(1));
    auto emb2 = torch::nn::EmbeddingBag(torch::nn::EmbeddingBagOptions(2, 2).max_norm(2).norm_type(2.5).scale_grad_by_freq(true).sparse(false).mode(torch::kSum).padding_idx(1));
    
    auto options = torch::TensorOptions().dtype(torch::kInt64);
    torch::Tensor t = torch::tensor({0,1,2,3,4,5,6,7,8,9}, options);
    torch::Tensor t2 = torch::tensor({{0,1,2},{0,1,4}}, options);
    torch::Tensor tharray = torch::from_blob(array, {3}, options);
    torch::Tensor offset2 = torch::from_blob(offset, {3}, options);
    
}   
