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
#include "data.h"
#include <algorithm>
#include <map>
#include <utility>
#include <map>
#include <list>
#include <fstream>
using namespace std;

bool compare(const pair<nvm_addr, int>  &p1,const pair<nvm_addr, int> &p2){
    return p1.second < p2.second;
}



void cache::GC_ondemand( ){
    vector<pair<nvm_addr, int>> cs_list;
    int err;
    for (int i = 0; i < geo->l.npugrp * geo->l.npunit; i++){
		for(int j = 0; j < geo->l.nchunk;j++){
            nvm_addr addr;
            if (chunkstate[i][j].invalid_page_num > 0){
                addr.val = 0;
                addr.l.pugrp = i / geo->l.npunit;
                addr.l.punit = i % geo->l.npunit;
                addr.l.chunk = j;
                //cs_list[addr] = chunkstate[i][j].invalid_page_num;
                cs_list.push_back(make_pair(addr, chunkstate[i][j].invalid_page_num ));
                if(chunkstate[i][j].invalid_page_num == geo->l.nsectr){
                        nvm_addr gc[1];
                        gc[0] = addr;
                        err = nvm_cmd_erase(dev,gc, 1 ,NULL,0x0,NULL);
                        if(err) cout<<"nvm_cmd_erase err"<<endl;
                        chunkstate[i][j].invalid_page_num = 0;
                        delete [] chunkstate[i][j].sector;
                        return;
                }
            }

		}
	} 
    sort(cs_list.begin(), cs_list.end(), compare);

    auto it = cs_list.rbegin();
    nvm_addr gc_addr = it->first;
    int count = 0;
    nvm_addr *reclaim_page = new nvm_addr[ws_opt];
    nvm_addr src;
    




    for (int k = 0; k < sector_num; k++){
        
        
        if(chunkstate[gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit][gc_addr.l.chunk].sector[k] == 1){
            reclaim_page[count] = gc_addr;
            reclaim_page[count].l.sectr = k;
            count ++;
            chunkstate[gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit][gc_addr.l.chunk].sector[k] = 0;
             

        }
        if (count == ws_opt){
            struct nvm_addr *pa;
            pa = new nvm_addr[ws_opt];
            struct nvm_addr addr;
            uint32_t wp = 0;
            addr.val = 0;
            find_chunk(addr, wp);
            for (int i = 0; i < ws_opt; i++){

                pa[i].val = addr.val;
                pa[i].l.sectr = wp + i;
                for (int x = 0; x < get_table_num(); x++){
                    for (int y = 0; y < get_size() / geo->l.nbytes; y = y + vector_page_offset){
                        if(transfer_PA(y, x).val == reclaim_page[i].val){
                            emb_vec[x][y].update_addr.val = pa[i].val;
                        }
                    }
                }

            }
 
            char *buf;
            buf = new char [ws_opt*byte_num];
            err = nvm_cmd_read(dev, reclaim_page , count,
                                buf, NULL,
                                0x0, NULL);
            if(err) cout<<"nvm_cmd_read err"<<endl;
            err = nvm_cmd_write(dev, pa, ws_opt, buf,
                                        NULL, 0x0, NULL);
              mark_sectors_valid(pa, ws_opt);
            if(err) cout<<"nvm_cmd_write err"<<endl;
            delete [] buf;
            delete [] pa;
            count = 0;
        }
        
    }
    struct nvm_addr *pa;
    pa = new nvm_addr[ws_opt];
    struct nvm_addr addr;
    uint32_t wp = 0;
    addr.val = 0;
    find_chunk(addr, wp);
    for (int i = 0; i < ws_opt; i++){
        pa[i].val = addr.val;
        pa[i].l.sectr = wp + i;
        if(i < count){
            for (int x = 0; x < get_table_num(); x++){
                for (int y = 0; y < get_size() / geo->l.nbytes; y = y + vector_page_offset){
                    if(transfer_PA(y, x).val == reclaim_page[i].val){
                        emb_vec[x][y].update_addr.val = pa[i].val;
                    }
                }
            }
        }
    }
    char *buf;
    buf = new char [ws_opt*byte_num];
    err = nvm_cmd_read(dev, reclaim_page , count,
                        buf, NULL,
                        0x0, NULL);
    if(err) cout<<"nvm_cmd_read err"<<endl;
    err = nvm_cmd_write(dev, pa, ws_opt, buf,
                                NULL, 0x0, NULL);
    
    if(err) cout<<"nvm_cmd_write err"<<endl;
      mark_sectors_valid(pa, ws_opt);
    nvm_addr gc[1];
    gc[0] = gc_addr;
    
    err = nvm_cmd_erase(dev,gc, 1 ,NULL,0x0,NULL);
    if(err) cout<<"nvm_cmd_erase err"<<endl;
    cout<<"nvm_cmd_erase: "<<gc_addr.l.chunk<<endl;
    delete [] buf;
    delete [] pa;
    delete [] reclaim_page;
    delete [] chunkstate[gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit][gc_addr.l.chunk].sector;
    chunkstate[gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit][gc_addr.l.chunk].sector = NULL;
    chunk_used --;
    chunkstate[gc_addr.l.pugrp * geo->l.npunit + gc_addr.l.punit][gc_addr.l.chunk].invalid_page_num = 0;
}


void cache::find_chunk(nvm_addr &addr, uint32_t &wp){
    bool find = 0;
    struct nvm_spec_rprt *rprt = NULL;
    
    for (int i = 0; i < geo->l.npugrp * geo->l.npunit; i++){
            addr.l.pugrp = i/geo->l.npunit;
            addr.l.punit = i%geo->l.npunit;

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
        
    }
    if (find == 0){
        wp = 0;

        rprt = NULL;

        for (uint32_t j = 0; j < geo->l.nchunk ; j++){

            for (;count_sector < geo->l.npugrp * geo->l.npunit; count_sector++){
                addr.l.pugrp = count_sector/geo->l.npunit;
                addr.l.punit = count_sector%geo->l.npunit;
                rprt = nvm_cmd_rprt(dev,&addr,0x0,NULL);
                if (rprt->descr[j].cs != NVM_CHUNK_STATE_FREE){
                    continue;
                }
                addr.l.chunk = j;
                cout<<"use free chunk:"<<addr.l.pugrp<<" "<<addr.l.punit<<" "<<addr.l.chunk<<endl;
                chunk_used++;
                return;
            }
            if (count_sector ==  geo->l.npugrp * geo->l.npunit){
                count_sector = 0;
            }
        }

    }
}


void cache::Reclaim_vector_page(int table_ID){

    
    int err;


    list<int> &lru  = lru_main[table_ID].lru;
	map<int,decltype(lru.begin()) > &h_map = lru_main[table_ID].hash_table;
    // if (lru.size() < ws_opt) {
      //  return;
   // }
    vector<int> reclaim;
    size_t ws_opt = nvm_dev_get_ws_opt(dev);
    int i = 0;
    
    for (auto it = lru.rbegin(); it != lru.rend(); it++){


		reclaim.push_back(*it);
		h_map.erase(*it);
		i++;    
        
        if (i== ws_opt)
            break;

    }

	for (int i = 0 ; i < ws_opt; i++){
		
		lru.pop_back();	
	}
    //GC_ondemand: when used chunk is higher than watermark,
//    then GC the most valid page chunk.
    //


    //while(chunk_used > (geo->l.npugrp * geo->l.npunit * geo->l.nchunk* GC_threshold)){
        //GC_ondemand();
    //}
    // if(chunk_used > 260){
    //     GC_ondemand();
    // }





    //concat_vector
    int j;
    char *write_buff = new char [ws_opt*byte_num];

    for (int i = 0; i < ws_opt; i++){
        char *emb;
        DATA t(emb_vec[table_ID][reclaim[i]].data, vector_page_offset, get_dimention());
        t.transfer_byte();
        t.transfer_char1();
        emb = t.get_char1();
        int k=0;
		for (j = i*t.getsize_char(); j < (i+1)*t.getsize_char();j++){
			write_buff[j] = emb[k];
			k++;
		}
	    emb_vec[table_ID][reclaim[i]].data = NULL;
        delete [] emb;
    }


    struct nvm_addr *pa;
    pa = new nvm_addr[ws_opt];
    struct nvm_addr addr;
    uint32_t wp = 0;
    addr.val = 0;
    find_chunk(addr, wp);



    for (int i = 0; i < ws_opt; i++){
        nvm_addr old_addr = transfer_PA(reclaim[i]*vector_page_offset, table_ID);
        if (chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].invalid_page_num == 0 ){
            chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].sector = new bool[sector_num];
			for (int k = 0; k < sector_num; k++){
				chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].sector[k] = 1;
			}       
        }
        chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].sector[old_addr.l.sectr] = 0;
        chunkstate[old_addr.l.pugrp * geo->l.npunit + old_addr.l.punit][old_addr.l.chunk].invalid_page_num ++;
        mark_sector_invalid(old_addr);

        pa[i].val = addr.val;
        pa[i].l.sectr = wp + i;
        // update vectors new location
        emb_vec[table_ID][reclaim[i]].update_addr.val = pa[i].val;
        
    }
    //write to ocssd
    

    err = nvm_cmd_write(dev, pa, ws_opt,
						    write_buff , NULL,
						    0x0, NULL);
    if (err) {
        perror("nvm_cmd_write");
    }
      mark_sectors_valid(pa, ws_opt);
	page_count -= ws_opt;
    delete [] write_buff;

    write_buf[pa[0].l.pugrp * geo->l.npunit + pa[0].l.punit].write_page_count += ws_opt;
}
void cache::lru_page(vector<int> &vectors, int table_ID){
    list<int> &lru  = lru_main[table_ID].lru;
	map<int,decltype(lru.begin()) > &h_map = lru_main[table_ID].hash_table;
    for(int i = 0; i < vectors.size(); i++){
        
		auto ret = h_map.find(vectors[i]/vector_page_offset);


        if (ret != h_map.end()){
            
            auto ret2 = ret->second;


            lru.splice(lru.begin(),lru,ret2);
        }
        else{
            lru.insert(lru.begin(), vectors[i]/vector_page_offset);
            h_map.insert(make_pair(vectors[i]/vector_page_offset, lru.begin()));
        }





		//auto ret = std::find(lru.begin(),lru.end(),vectors[i]/vector_page_offset);

        //if (ret != lru.end()){

            //lru.erase(ret);
            //lru.insert(lru.begin(), vectors[i]/vector_page_offset);
        //}
        //else{
            //lru.insert(lru.begin(), vectors[i]/vector_page_offset);
        //}
    }
}





float** cache::get_emb_vectors(vector<int> &vectors, int table_ID){
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






struct nvm_addr cache::transfer_PA(int vec_ID, int table_ID){
    if(emb_vec[table_ID][vec_ID/vector_page_offset].update_addr.val != 0)
        return emb_vec[table_ID][vec_ID/vector_page_offset].update_addr;
    else
        return transfer_PA_from_init(vec_ID, table_ID);
}




void cache::read(vector<int> &read_vectors, int table_ID){
	  if (read_vectors.empty()) return;
    	vector<int> vectors;

    int err;
    char *read_buf;
    //size_t ws_opt = nvm_dev_get_ws_opt(dev);

    struct nvm_addr *pa;
    struct nvm_addr pa_check;

    vector<int> ID_rec;
    



    




    // check vectors already in cache?
    for (int i = 0; i < read_vectors.size(); i++){
        if (emb_vec[table_ID][read_vectors[i]/vector_page_offset].data == NULL){
            vectors.push_back(read_vectors[i]);
        }
    }



    if(vectors.size()==0) return;


    int lookup = vectors.size();
    pa = new nvm_addr[lookup];

    // transfer pa
    int j = 0;
	for (int i = 0; i < vectors.size(); i++){

        if( i>0 && transfer_PA(vectors[i],table_ID).val == pa_check.val){
            lookup--;
            continue;
        }
        ID_rec.push_back(vectors[i]/vector_page_offset);

		pa[j] = transfer_PA(vectors[i],table_ID);
        write_buf[pa[j].l.pugrp * geo->l.npunit + pa[j].l.punit].read_page_count++;
        pa_check.val = pa[j].val;               
        j++;
	}
   

	page_count += lookup;
    read_buf = new char[lookup * byte_num];

    int size = lookup*byte_num/(get_dimention()*sizeof(float));



    //read ocssd
    int ofz = 0;
    int buf_ofz = ofz*byte_num;
    while (lookup > ws_opt){
        struct nvm_addr *pa_read;
        pa_read = new nvm_addr[ws_opt];
        int j=0;
        for (int i=ofz; i< ofz+ws_opt;i++){
            pa_read[j] = pa[i];
            j++;
        }
        buf_ofz = ofz*byte_num;
        err = nvm_cmd_read(dev, pa_read, ws_opt,
                        read_buf+ buf_ofz, NULL,
                        0x0, NULL);
        
        if (err) {
            perror("nvm_cmd_read");
        }
        lookup = lookup-ws_opt;
        ofz = ofz + ws_opt;
        delete [] pa_read;
    }

    struct nvm_addr *pa_read;
    pa_read = new nvm_addr[lookup];
    j = 0;
    for (int i=ofz; i< ofz+lookup;i++){
        pa_read[j] = pa[i];
        j++;    
    }
    buf_ofz = ofz*byte_num;
    err = nvm_cmd_read(dev, pa_read, lookup,
                        read_buf+buf_ofz, NULL,
                        0x0, NULL);
        
    if (err) {
        perror("nvm_cmd_read");
    }

    delete [] pa_read;
    delete [] pa;


    //transfer data to float


	DATA t(read_buf,size,get_dimention());
    t.transfer_char2();
    t.transfer_float();
    
    int count  = 0 ;

   
    for (int  i =0; i < size; i = i + vector_page_offset){
        
        int ID = ID_rec[count];
        emb_vec[table_ID][ID].data = new float*[vector_page_offset];
        for (int j = 0; j < vector_page_offset; j++){
        
            emb_vec[table_ID][ID].data[j] = t.getdata_float()[count*vector_page_offset+j];
            //cout<<emb_vec[table_ID][ID].data[j][0] <<endl;
        }
        count ++;
    }


    //delete [] read_buf;
    //t.del_emb_data();

}





// int main(int argc, char **argv)
// {


// 	// embedding_table_init emb2(32,500000,64);
// 	// emb2.write();
// 	// struct nvm_addr **pa = emb2.get_pa_table();
// 	// emb2.pa_init();

//     cache c(32,500000,64);
//     //c.write();
//     c.pa_init();
//     vector<int> vectors{1,2,3,17,33, 64, 96,128, 256, 512,1024,2048,4096,17810, 19799, 49745, 50202, 59552, 
//   66988, 81514, 83953};

//     float **embedding_vec;
//     float **en;
//     for(int i = 0;  i < 32 ; i ++){
//     	embedding_vec = c.get_emb_vectors(vectors,i);
//     } 
//     c.Reclaim_vector_page(0);

// }
