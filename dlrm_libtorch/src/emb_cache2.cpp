#include <iostream>
#include <stdio.h>
#include <liblightnvm.h>
#include <liblightnvm_spec.h>
#include <liblightnvm_cli.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <list>
#include "data2.h"
#include <algorithm>
#include <map>
#include <utility>
#include <unordered_map>


using namespace std;

bool compare(const pair<nvm_addr, int>  &p1,const pair<nvm_addr, int> &p2){
    return p1.second < p2.second;
}
bool compare2(const pair<int, int>  &p1,const pair<int, int> &p2){
    return p1.first < p2.first;
}

nvm_addr cache2::GC_vectors(vector<int> vectors, int table_ID){
    int lookup = vectors.size();
    struct nvm_addr pa_check;
    struct nvm_addr *pa;
    pa = new nvm_addr[lookup];


    int j = 0;
    for (int i = 0; i < vectors.size(); i++){

        if( i>0 && transfer_PA(vectors[i],table_ID).val == pa_check.val){
            lookup--;
            continue;
        }
        


        pa[j] = transfer_PA(vectors[i],table_ID);
        
        pa_check.val = pa[j].val;               
        j++;
    }



    char *read_buf;
    read_buf = new char[lookup * byte_num];

    int size = lookup*vector_page_offset;


    int err;
    int ofz = 0;
    int buf_ofz = ofz*byte_num;
    while (lookup > 0){
        struct nvm_addr *pa_read;
        buf_ofz = ofz*byte_num;
        if (lookup >= ws_opt){

            pa_read = new nvm_addr[ws_opt];
            int j=0;
            for (int i=ofz; i< ofz+ws_opt;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, ws_opt,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }
        else{
            
            pa_read = new nvm_addr[lookup];
            int j=0;
            for (int i=ofz; i< ofz+lookup;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, lookup,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }

        if (err) {
            perror("nvm_cmd_read");
        }
        lookup = lookup-ws_opt;
        ofz = ofz + ws_opt;
        delete [] pa_read;
    }
    delete [] pa;

    nvm_addr chunk_addrs;
    find_free_chunk(chunk_addrs, table_ID);
    for (size_t sectr = 0; sectr < geo->l.nsectr; sectr += ws_opt) {
        size_t buf_ofz = sectr * geo->l.nbytes;
        struct nvm_addr addrs[ws_opt];
        
        for (size_t aidx = 0; aidx < ws_opt; ++aidx) {
            addrs[aidx].val = chunk_addrs.val;
            addrs[aidx].l.sectr = sectr + aidx;
        }	


        
        err = nvm_cmd_write(dev, addrs, ws_opt,
                    read_buf + buf_ofz, NULL,
                    0x0, NULL);

        if (err) {
            perror("nvm_cmd_write");
        }

    }
    delete [] read_buf;
    return chunk_addrs;

}




nvm_addr cache2::GC_pa_table( int vec_start, nvm_addr gc_addr){

    vector <int> vectors ;
    int table_ID = gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit;
    
    
    for (int k = vec_start; k < vec_start + sector_num*vector_page_offset; k++){
        vectors.push_back(k);
    }

    nvm_addr addr = GC_vectors(vectors, table_ID);

    for (int k = vec_start; k < vec_start + sector_num*vector_page_offset; k++){
        auto it = emb_vec[table_ID][k/vector_page_offset].cold_table.find(k);
        if( it != emb_vec[table_ID][k/vector_page_offset].cold_table.end()){
            // invalid cold_table vectors not need for hot_table vectors
            nvm_addr addr;
            addr.val = it->second.val;
            chunkstate[addr.l.pugrp * geo->l.npunit + addr.l.punit][addr.l.chunk].invalid_vector_num++;
            emb_vec[table_ID][k/vector_page_offset].cold_table.erase(k);
        }
        
        vectors.push_back(k);
    }
    
    return addr;
}
void cache2::GC_ondemand( ){
    vector<pair<nvm_addr, int>> cs_list;
    int err;
    for (int i = 0; i < geo->l.npugrp * geo->l.npunit; i++){
		for(int j = 0; j < geo->l.nchunk;j++){
            nvm_addr addr;
            if (chunkstate[i][j].invalid_vector_num > 0){
                addr.val = 0;
                addr.l.pugrp = i / geo->l.npunit;
                addr.l.punit = i % geo->l.npunit;
                addr.l.chunk = j;
                //cs_list[addr] = chunkstate[i][j].invalid_page_num;
                cs_list.push_back(make_pair(addr, chunkstate[i][j].invalid_vector_num ));
            }

		}
	} 
    

    auto it = max_element(cs_list.begin(), cs_list.end(), compare);
    nvm_addr gc_addr = it->first;
    bool check_reclaim =0;
    int vec_start = 0;
    int table_ID = gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit;
    int i;
    int j;
    //check chunk if it is store for origin data or update data
    for (i = 0; i < geo->l.npugrp * geo->l.npunit; i++){
		for(j = 0; j < write_count;j++){
            if (pa_table[i][j].val == gc_addr.val){
                check_reclaim = 1;
                vec_start = j * ( sector_num * byte_num /(get_dimention()*sizeof(float)));
                break;
            }
			
		}
        if (check_reclaim == 1){
                break;
        }
	} 
    //reclaim pa_table(origin data) is not truely gc because it will need additional write
    if(check_reclaim == 1){
        pa_table[i][j] = GC_pa_table( vec_start, gc_addr);
        nvm_addr gc[1];
        gc[0] = gc_addr;
        err = nvm_cmd_erase(dev,gc, 1 ,NULL,0x0,NULL);
        if(err) cout<<"nvm_cmd_erase err"<<endl;
        cout<<"nvm_cmd_erase: "<<gc_addr.l.chunk<<endl;
    }
    else{//reclaim the chunk(update data) and reverse table O(n^3)n
        vector <int> vectors;
        for (int i = 0; i < sector_num; i++){
            nvm_addr addr;
            addr.val = gc_addr.val;
            addr.l.sectr = i;
            auto it = write_buf[table_ID].reverse_table.find(addr.val);
            if (it != write_buf[table_ID].reverse_table.end()){
                for (auto p = it->second.begin(); p != it->second.end(); p++){
                    auto it2 = emb_vec[table_ID][*p/vector_page_offset].hot_table.find(*p);
                    if (it2 != emb_vec[table_ID][*p/vector_page_offset].hot_table.end()){
                        if(it2->second.val == addr.val){
                            vectors.push_back(*p);
                            continue;
                        }
                    }
                    auto it3 = emb_vec[table_ID][*p/vector_page_offset].cold_table.find(*p);
                    if (it3 != emb_vec[table_ID][*p/vector_page_offset].cold_table.end()){
                        if(it3->second.val == addr.val){
                            vectors.push_back(*p);
                        }
                    }
                }
                write_buf[table_ID].reverse_table.erase(addr.val);
            }
        }
        get_emb_vectors(vectors, table_ID);
        nvm_addr gc[1];
        gc[0] = gc_addr;
        err = nvm_cmd_erase(dev,gc, 1 ,NULL,0x0,NULL);
        if(err) cout<<"nvm_cmd_erase err"<<endl;
        cout<<"nvm_cmd_erase: "<<gc_addr.l.chunk<<endl;
    }
    free_chunk++;
}


void cache2::find_chunk(nvm_addr &addr, uint32_t &wp, int table_ID){
    bool find = 0;
    struct nvm_spec_rprt *rprt = NULL;
    int table_countx = table_ID;

    addr.l.pugrp = table_countx/geo->l.npunit;
    addr.l.punit = table_countx%geo->l.npunit;

    rprt = nvm_cmd_rprt(dev,&addr,0x0,NULL);
    for (uint32_t j = 0; j < rprt->ndescr; j++){

        
        if (rprt->descr[j].cs != NVM_CHUNK_STATE_OPEN){
            continue;
        }
        addr.l.chunk = j;
        wp = rprt->descr[j].wp;
        find = 1;
        return;
    }
    
    
    if (find == 0){
        wp = 0;

        rprt = NULL;

        for (uint32_t j = 0; j < geo->l.nchunk; j++){
		
            addr.l.pugrp = table_countx/geo->l.npunit;
            addr.l.punit = table_countx%geo->l.npunit;
            rprt = nvm_cmd_rprt(dev,&addr,0x0,NULL);
            if (rprt->descr[j].cs != NVM_CHUNK_STATE_FREE){
                continue;
            }
            addr.l.chunk = j;
            free_chunk--;
            //chunk_used++;
            return;
	    }

    }
}

void cache2::Reclaim_cold_table(int table_ID, int v ){ 
    struct nvm_addr *pa;
    vector<int> vs;
    int err;

    v = v / vector_table_offset ;
    for (int i = 0; i < ws_opt; i++){
        for (int j = 0; j < vector_page_offset; j++ ){
            vs.push_back(v++);
        }
    }

    int lookup = vs.size();
    pa = new nvm_addr[lookup];
    nvm_addr pa_check;
    // transfer pa
    int j = 0;
	for (int i = 0; i < vs.size(); i++){

        if( i>0 && transfer_PA(vs[i],table_ID).val == pa_check.val){
            lookup--;
            continue;
        }



		pa[j] = transfer_PA(vs[i],table_ID);
        
        pa_check.val = pa[j].val;               
        j++;
	}
   


    char *read_buf;
    read_buf = new char[lookup * byte_num];

    int size = lookup*vector_page_offset;



    //read ocssd
    int ofz = 0;
    int buf_ofz = ofz*byte_num;
    while (lookup > 0){
        struct nvm_addr *pa_read;
        buf_ofz = ofz*byte_num;
        if (lookup >= ws_opt){

            pa_read = new nvm_addr[ws_opt];
            int j=0;
            for (int i=ofz; i< ofz+ws_opt;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, ws_opt,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }
        else{
            
            pa_read = new nvm_addr[lookup];
            int j=0;
            for (int i=ofz; i< ofz+lookup;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, lookup,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }

        if (err) {
            perror("nvm_cmd_read");
        }
        lookup = lookup-ws_opt;
        ofz = ofz + ws_opt;
        delete [] pa_read;
        
    }

    struct nvm_addr addr;
    uint32_t wp = 0;
    addr.val = 0;

    find_chunk(addr, wp, table_ID);


    for (int i = 0; i< ws_opt; i++){
        pa[i].val = addr.val;
        pa[i].l.sectr = wp + i;
    }
    err = nvm_cmd_write(dev, pa, ws_opt,
                        read_buf , NULL,
                        0x0, NULL);
    if (err) {
        perror("nvm_cmd_write");
    }
    int table_countx = table_ID * write_table;

    //???????????????????????????

}





void cache2::Reclaim_vector_page(int table_ID){

    
    int err;


    list<pair<int, int>> &lru  = lru_main[table_ID].lru;
    map<int,decltype(lru.begin()) > &h_map = lru_main[table_ID].hash_table;
    vector<pair<int, int>> &reclaim  = write_buf[table_ID].reclaim;
    list<pair<nvm_addr , int >> hot_page_list = write_buf[table_ID].hot_page_list;
    size_t ws_opt = nvm_dev_get_ws_opt(dev);


	list<pair<int, int>>::iterator it;
 // 全部開啟來LFU ，沒有LRU
 //   while (lru.size() > 0){
	    it = lru.end();
		--it;
   //     if(it->second > 0){

//			it->second--;
//			if (it->second > write_buf[table_ID].aging_rate){
//				it->second = it->second/2;
//			}
//			lru.splice(lru.begin(),lru,it);

//		}
//		else{
//			break;
//		}
                                    
  //  }
    int vec = it->first;

    vec = (vec/vector_page_offset)*vector_page_offset;

    for (int i = vec ; i < vec + vector_page_offset; i++){

        map<int,decltype(lru.begin()) >::iterator f;

        f = h_map.find(i);
        
        if (i == it->first ){
            vector<pair<int, int>>::iterator f2;

            f2 = std::find_if(reclaim.begin(),reclaim.end(),
                 [&i](const pair<int,int> p){
                    return p.first == i;
                }
            );
            
            if(f2 != reclaim.end()){
                continue;
            }
    
            reclaim.push_back(*it);
           /* 
            char *emb;
            DATA t(emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset], 1, get_dimention());
            t.transfer_float1_to_char1();
            emb = t.get_char1();
            
            for (int j = write_buf[table_ID].write_pad*t.getsize_char(), k = 0; j < (write_buf[table_ID].write_pad+1)*t.getsize_char();j++, k++){
                write_buf[table_ID].write_buf[j] = emb[k];
                k++;
            }
            write_buf[table_ID].write_pad++;
            delete [] emb;
            emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset] = NULL;
            */ 
           if (emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset] != NULL) {
 		   DATA t(emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset], 1, get_dimention());
    		t.transfer_float1_to_char1();
    		char *emb = t.get_char1();  // 獲取內部指針
    
    		int bytes_per_vector = get_dimention() * sizeof(float);
    int offset = write_buf[table_ID].write_pad * bytes_per_vector;
    
    // 使用 memcpy 而非手動迴圈，更安全高效
    memcpy(write_buf[table_ID].write_buf + offset, emb, bytes_per_vector);
    
    write_buf[table_ID].write_pad++;
    
    // 如果 DATA 析構函數負責釋放 char1，就不要在這裡 delete
    // 移除: delete [] emb;
    
    // 設置指針為 NULL
    emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset] = NULL;
}
            if (reclaim.size() == ws_opt*vector_page_offset){
                write_buf[table_ID].write_pad = 0;
                break;
            }
            
        }
        else if (emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset] != NULL && f== h_map.end()) {
            delete [] emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset];
            emb_vec[table_ID][i/vector_page_offset].data[i%vector_page_offset] = NULL;
            
        }


    }

    h_map.erase(it->first);
    lru.pop_back();
    

    // delete [] emb_vec[table_ID][vec/vector_page_offset].data;
    // emb_vec[table_ID][vec/vector_page_offset].data = NULL;
    if (reclaim.size() == ws_opt*vector_page_offset){
            

        /*GC_ondemand: when used chunk is higher than watermark,
        then GC the most valid page chunk.
        */


        //if(free_chunk < geo->l.npugrp * geo->l.npunit * 0.1/* GC_threshold*/){
            //GC_ondemand();
        //}







        struct nvm_addr *pa;
        pa = new nvm_addr[ws_opt* vector_page_offset];
        struct nvm_addr addr;
        uint32_t wp = 0;
        addr.val = 0;
        find_chunk(addr, wp, table_ID);

 

        for (int i = 0; i < ws_opt; i++){
            for (int j= 0; j < vector_page_offset; j++){
                nvm_addr old_addr = transfer_PA(reclaim[i * vector_page_offset + j].first, table_ID);

                chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].invalid_vector_num ++;

            }
            

            pa[i].val = addr.val;
            
            pa[i].l.sectr = wp + i;
            
            int totalvalue = 0;
            for (int j= 0; j < vector_page_offset; j++){

                totalvalue += reclaim[i*vector_page_offset+j].second;
                
            }
            list<pair<nvm_addr , int >> ::iterator mintotal;

            mintotal = min_element(hot_page_list.begin(), hot_page_list.end(), compare);
            
            // update vectors new location
            if (hot_page_list.size() < hot_page_count ){
                for (int j= 0; j < vector_page_offset; j++){
                    emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].hot_table[reclaim[i*vector_page_offset+j].first] = pa[i] ;  
                    write_buf[table_ID].reverse_table[pa[i].val].push_back(reclaim[i*vector_page_offset+j].first);
                    
                }

                hot_page_list.push_back(make_pair(pa[i], totalvalue));
                
            }
            else if(totalvalue > mintotal->second){
                hot_page_list.erase(mintotal);
                
                for (int j= 0; j < vector_page_offset; j++){ 
                    emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].hot_table[reclaim[i*vector_page_offset+j].first] = pa[i] ;

                    write_buf[table_ID].reverse_table[pa[i].val].push_back(reclaim[i*vector_page_offset+j].first);
                    
                }
                hot_page_list.push_back(make_pair(pa[i], totalvalue));
                
            }
            else{
                // delete hot table old address
    
                for (int j= 0; j < vector_page_offset; j++){
                    auto it = emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].hot_table.find(reclaim[i*vector_page_offset+j].first);
                    if (it !=  emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].hot_table.end() ){

                        emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].hot_table.erase(reclaim[i*vector_page_offset+j].first);  
                        
                    }
                }

                // add new address to cold table
                for (int j= 0; j < vector_page_offset; j++){
                    emb_vec[table_ID][reclaim[i*vector_page_offset+j].first / vector_page_offset].cold_table[reclaim[i*vector_page_offset+j].first] = pa[i]; 

                    write_buf[table_ID].reverse_table[pa[i].val].push_back(reclaim[i*vector_page_offset+j].first);
                    

                }
            }
            
        }
        
        //write to ocssd
        err = nvm_cmd_write(dev, pa, ws_opt,
                                write_buf[table_ID].write_buf, NULL,
                                0x0, NULL);
        if (err) {
            perror("nvm_cmd_write");
        }
        write_buf[table_ID].write_page_count += ws_opt;
        page_count -= ws_opt;
        //delete [] pa;
        // for (int i = 0; i < reclaim.size(); i++){
        //     int temp = reclaim[i].first;
        //     auto f = std::find_if(lru.begin(),lru.end(),
        //          [&temp](const pair<int,int> p){
        //             return p.first == temp;
        //         }
        //     );
        //     lru.erase(f);

        // }

        vector<pair<int, int>>().swap(reclaim);
        
    }
    
 
  
}




void cache2::lru_page(vector<int> vectors, int table_ID){
    list<pair<int , int >> &lru  = lru_main[table_ID].lru;
    map<int,decltype(lru.begin()) > &h_map = lru_main[table_ID].hash_table;

    int vec2 = -1;
    for(auto i = vectors.begin(); i != vectors.end(); i ++){

        auto ret = h_map.find(*i);


        if (ret != h_map.end()){
            
            auto ret2 = ret->second;
            ++ret2->second;

            //lru.splice(lru.begin(),lru,ret2);
        }
        else{
            lru.insert(lru.begin(), make_pair(*i, 0));
            h_map.insert(make_pair(*i, lru.begin()));
        }
    }
    
    // for(auto i = vectors.begin(); i != vectors.end(); i ++){
    //     int t = *i;  
    //     auto ret = std::find_if(lru.begin(),lru.end(),
    //              [&t](const pair<int,int> p){
    //                 return p.first == t;
    //             }
    //         );

    //     if (ret != lru.end()){
    //         int temp = ++ret->second;
    //         lru.erase(ret);
    //         lru.insert(lru.begin(), make_pair(*i, temp));
    //     }
    //     else{
    //         lru.insert(lru.begin(), make_pair(*i, 0));
    //     }
    // }


}





float** cache2::get_emb_vectors(vector<int> &vectors, int table_ID){
    float **emb_vectors;

	
	
    read(vectors, table_ID);
    emb_vectors = new float *[vectors.size()];
    for (int i = 0; i < vectors.size(); i++){
        emb_vectors[i] = emb_vec[table_ID][vectors[i]/vector_page_offset].data[vectors[i]%vector_page_offset];
        //cout<<emb_vectors[i][1]<<endl;
    }
    lru_page(vectors, table_ID);
    return emb_vectors;
}

void cache2::transfer_vector(int &vec_ID, int &table_ID){
    table_ID = table_ID * write_table; 
    bool flag = 1 ;
	while (vec_ID > (get_size()/ write_table)){
		vec_ID -=  get_size()/ write_table;
		table_ID++;
	}

}




struct nvm_addr cache2::transfer_PA(int vec_ID, int table_ID){
    auto it = emb_vec[table_ID][vec_ID/vector_page_offset].hot_table.find(vec_ID);
    if( it != emb_vec[table_ID][vec_ID/vector_page_offset].hot_table.end())
        return it->second;
    else{
        auto it = emb_vec[table_ID][vec_ID/vector_page_offset].cold_table.find(vec_ID);
        if( it != emb_vec[table_ID][vec_ID/vector_page_offset].cold_table.end())
            return it->second;
        return transfer_PA_from_init(vec_ID, table_ID);
    }
}




void cache2::read(vector<int> read_vectors, int table_ID){
  if (read_vectors.empty()) return;
    vector<int> vectors;

    int err;
    char *read_buf;
    //size_t ws_opt = nvm_dev_get_ws_opt(dev);

   
    struct nvm_addr pa_check;

    vector<int> ID_rec;
    
    list<pair<int , int >> &lru  = lru_main[table_ID].lru;


    

    
    
    for (int i = 0; i < read_vectors.size(); i++){
        if (emb_vec[table_ID][read_vectors[i]/vector_page_offset].data == NULL){
            #pragma omp critical
            {
                vectors.push_back(read_vectors[i]);
            }
        }
        else if (emb_vec[table_ID][read_vectors[i]/vector_page_offset].data[read_vectors[i]%vector_page_offset] == NULL){
            #pragma omp critical
            {
                vectors.push_back(read_vectors[i]);
            }
        }
    }
    

    if(vectors.size()==0) return;


    int lookup = vectors.size();
    struct nvm_addr *pa;

    pa = new nvm_addr[lookup];
    map <uint64_t, bool> vector_map;
    // transfer pa
    int j = 0;
    for (int i = 0; i < vectors.size(); i++){
        uint64_t k = transfer_PA(vectors[i],table_ID).val;

        auto it = vector_map.find(k);
        if(it != vector_map.end()){
            lookup--;
            continue;
        }
        
        #pragma omp critical
        {
            vector_map.insert(make_pair(k,0));
            ID_rec.push_back(vectors[i]/vector_page_offset);
        }
        pa[j] = transfer_PA(vectors[i],table_ID);
        
        pa_check.val = pa[j].val;               
        j++;
    }
   
  if (read_vectors.size() != lookup) {
    write_buf[table_ID].aging_rate = read_vectors.size()/(read_vectors.size()-lookup);
} else {
    // 所有向量都需要讀取，設置一個合理的默認值
    write_buf[table_ID].aging_rate = 1; // 或更高的值，取決於老化策略
}  
//	write_buf[table_ID].aging_rate = read_vectors.size()/(read_vectors.size()-lookup);
    write_buf[table_ID].read_page_count += lookup; 
    read_buf = new char[lookup * byte_num];

    int size = lookup*vector_page_offset;



    //read ocssd
    int ofz = 0;
    int buf_ofz = ofz*byte_num;
    while (lookup > 0){
        struct nvm_addr *pa_read;
        buf_ofz = ofz*byte_num;
        if (lookup >= ws_opt){

            pa_read = new nvm_addr[ws_opt];
            int j=0;
            for (int i=ofz; i< ofz+ws_opt;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, ws_opt,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }
        else{
            pa_read = new nvm_addr[lookup];
            int j=0;
            for (int i=ofz; i< ofz+lookup;i++){
                pa_read[j] = pa[i];
                j++;
            }
            err = nvm_cmd_read(dev, pa_read, lookup,
                            read_buf+ buf_ofz, NULL,
                            0x0, NULL);
        }
        //cout<<"lookup: "<<lookup<<endl;
        //cout<<"ws_opt: "<<ws_opt<<endl;
        //cout<<"ofz: "<<ofz<<endl;
        //cout<<"byte_num: "<<byte_num<<endl;
        if (err) {
            perror("nvm_cmd_read");
        }
        lookup = lookup-ws_opt;
        ofz = ofz + ws_opt;
        delete [] pa_read;
    }





    //transfer data to float


	DATA t(read_buf,size,get_dimention());
    //t.transfer_char2();
    //t.transfer_float();
    t.transfer_char1_to_float2();

    int count  = 0 ;

   
    for (int  i =0; i < size; i = i + vector_page_offset){
        auto it = write_buf[table_ID].reverse_table.find(pa[count].val);
        if (it != write_buf[table_ID].reverse_table.end()){
            for (int j = 0; j < vector_page_offset; j++){
                if (emb_vec[table_ID][it->second[j]/vector_page_offset].data == NULL){
                    emb_vec[table_ID][it->second[j]/vector_page_offset].data = new float*[vector_page_offset];
                }
                emb_vec[table_ID][it->second[j]/vector_page_offset].data[it->second[j]%vector_page_offset] = t.getdata_float()[count*vector_page_offset+j];
                //cout<<emb_vec[table_ID][it->second[j]/vector_page_offset].data[it->second[j]%vector_page_offset][0] <<endl;
            }
        }
        else{
            int ID = ID_rec[count];
            emb_vec[table_ID][ID].data = new float*[vector_page_offset];
            for (int j = 0; j < vector_page_offset; j++){
            
                emb_vec[table_ID][ID].data[j] = t.getdata_float()[count*vector_page_offset+j];
                //cout<<emb_vec[table_ID][ID].data[j][0] <<endl;
            }
        }
        count ++;
    }

    delete [] pa;
    //delete [] read_buf;
    //t.del_emb_data();

}




// int main(int argc, char **argv)
// {


// 	// embedding_table_init emb2(32,500000,64);
// 	// emb2.write();
// 	// struct nvm_addr **pa = emb2.get_pa_table();
// 	// emb2.pa_init();

//     cache2 c(32,500000,64);
//     //c.write();
//     c.pa_init();
//     srand(time(NULL));

//      vector<int> vectors2;
//     for (int i = 0; i < 200; i++){
//         vectors2.push_back(rand()%500000);
//     }

// //     vector<int> vectors{1,2,3,17,33, 64, 96,128, 256, 512,1024,2048,4096,17810, 19799, 49745, 50202, 59552, 
// //    66988, 81514, 83953};
//     //vector<int>vectors2{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
//     //random_shuffle(vectors.begin(), vectors.end());
//     //sort(vectors.begin(), vectors.end());
//     float **embedding_vec;
//     float **en;
 
//    // embedding_vec = c.get_emb_vectors(vectors,0);
//     //embedding_vec = c.get_emb_vectors(vectors,0);
//     sort(c.lru_main[1].begin(), c.lru_main[1].end(), compare2); 

//     embedding_vec = c.get_emb_vectors(vectors2,1);
    
//     embedding_vec = c.get_emb_vectors(vectors2,1);
    
//     sort(c.lru_main[1].begin(), c.lru_main[1].end(), compare2); 
//     vector<pair<int ,int>>test;
//     while(c.lru_main[1].size()>50)
//         c.Reclaim_vector_page(1);
//     embedding_vec = c.get_emb_vectors(vectors2,1);
//     for (auto i = c.lru_main[0].begin(); i!= c.lru_main[0].end(); i++ ){
//         if (i->second >= 0 ){
//             test.push_back(*i);
//         }
//     }

//     cout<<"end"<<endl;
// }
