#ifndef DATA2_H
#define DATA2_H
#include <iostream>
#include<iomanip>
#include <stdio.h>
#include <liblightnvm.h>
#include <liblightnvm_spec.h>
#include <liblightnvm_cli.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <map>
#include <list>
#include <unordered_map>
#include <utility>

using namespace std;



struct emb_vec_page{
	emb_vec_page(): data(){}
	map<int,nvm_addr > hot_table;
	map<int,nvm_addr > cold_table;
	float **data;

};

struct chunk_state{
	// chunk_state(): invalid_vector_num(0){};
	chunk_state(): invalid_vector_num(0), sector_valid(0), sector_invalid(0){};
	int invalid_vector_num;
	int sector_valid;
	int sector_invalid;
	vector<uint8_t>sector_states;   // 0=unused,1=valid,2=invalid
};

struct write_buffer{
	write_buffer():write_pad(0),read_page_count(0),write_page_count(0){};
	list<pair<nvm_addr , int >> hot_page_list;
	char *write_buf;
	int write_pad;
	vector<pair<int, int>> reclaim;
	map<uint64_t,vector<int>> reverse_table;// <nvm_addr, vectors>
	int read_page_count;
	int write_page_count;
	int aging_rate;
};

struct lrumain{
	list<pair<int , int >> lru;
	map<int,decltype(lru.begin()) > hash_table;
};

class DATA{
	public:
		void transfer_float2_to_char1();
		void transfer_char1_to_float2();
		void initial();
		void transfer_byte();
		void transfer_char1();
		void transfer_char2();
		void transfer_float1();
		void transfer_float1_to_char1();
		float **getdata_float();
		char **getdata_char2();
		
		//use for writing ocssd 
		
		//use for debug
		float **transfer_float_debug(char** emb);
		//use for reading
		float **transfer_float();
		int getsize_char(){
			return m_dimention * m_size * sizeof(float) ;
		}
		void del_emb_data(){
			for (int i = 0; i < m_size; i++){
				delete [] emb_data[i];
			}
			delete [] emb_data;
		}
		int get_size(){return m_size;}
		int get_dimention(){return m_dimention;}
		~DATA(){
			// for (int i = 0; i < m_size; i++){
			// 	delete [] emb_data_char[i];
			// }
			// delete [] emb_data_char;
		}
		
		//for vectors transfer
		DATA(int size,  int dimention):m_size(size), m_dimention(dimention){};
		DATA(char **emb, int size,  int dimention):m_size(size), m_dimention(dimention), emb_data_char(emb){};
		DATA(float **emb, int size,  int dimention):m_size(size), m_dimention(dimention), emb_data(emb){};
		DATA(float *emb, int size,  int dimention):m_size(size), m_dimention(dimention), float1(emb){};
		DATA(char *emb, int size,  int dimention):m_size(size), m_dimention(dimention), char1(emb){};
		char *get_char1(){
			return char1;
		}
		float *get_float1(){
			return float1;
		}
	private:
		int m_dimention;
		int m_size;
		float **emb_data;
		char** emb_data_char;
		char * char1;
		float * float1;

};


class embedding_table_init2:protected DATA{
	public:


		//debug
		void pa_init();
		//debug
		int write_table;
		int write_count;

		struct nvm_addr transfer_PA_from_init(int vec_ID, int table_ID);
		struct nvm_addr **get_pa_table(){
			return pa_table;
		}
		int get_table_num(){
			return m_table_num*write_table;
		}
		int get_total_write_size(){
			return getsize_char()/write_table;
		}
		char *get_emb_table_write(){
			return emb_table;
		}
		int init_sector;
		embedding_table_init2(int table_num, int size, int dimention):m_table_num(table_num),DATA(size, dimention){
			
			init_sector =0;
			dev = nvm_dev_open("/dev/nvme0n1");
			geo = nvm_dev_get_geo(dev);
			ws_opt = nvm_dev_get_ws_opt (dev);
			sector_num = geo->l.nsectr;
			byte_num = geo->l.nbytes;
			write_table  = geo->l.npugrp * geo->l.npunit/table_num;
			if (write_table < 1) {
    write_table = 1; // 確保至少為1，避免除零錯誤
}
			chunkstate = new chunk_state*[geo->l.npugrp * geo->l.npunit];
			for (int i = 0 ; i <  geo->l.npugrp * geo->l.npunit ;i++){
				chunkstate[i] = new chunk_state[geo->l.nchunk];
				    for (uint32_t j=0;j<geo->l.nchunk;j++){
       					 chunkstate[i][j].sector_states.assign(geo->l.nsectr,0);
   				 }
			}
			vector_page_offset = geo->l.nbytes / (get_dimention() * sizeof(float));
			chunk_used = 0;
			GC_threshold =0.1;
			free_chunk =  0;
			struct nvm_spec_rprt *rprt = NULL;
			rprt = NULL;
			nvm_addr addr;
			addr.val = 0;
			for (int i = 0 ; i < geo->l.npugrp * geo->l.npunit; i ++){
				

				for (uint32_t j = 0; j <  geo->l.nchunk; j++){
					addr.l.pugrp = i/geo->l.npunit;
					addr.l.punit = i%geo->l.npunit;
					//addr.l.chunk = j;
					rprt = nvm_cmd_rprt(dev,&addr,0x0,NULL);
					if (rprt->descr[j].cs != NVM_CHUNK_STATE_FREE){
						continue;
					}
					
					free_chunk ++;
				}
			}
		
			
		};
		void init_free_page();
		int free_chunk;
		struct nvm_addr  *write_sync(char *write_buf,int byte, int write_count);
		void concat_emb();
		void divide_emb();
		void write();
		void find_free_chunk(nvm_addr &nvm_addr,int write_count);
		size_t ws_opt;
		int sector_num;
		int byte_num;
		struct nvm_dev *dev;
		const struct nvm_geo *geo;
		struct chunk_state **chunkstate;
		int vector_page_offset;
		int chunk_used;
		float GC_threshold;
		void dump_chunk_sector_stats(const std::string &filename);
		  void mark_sectors_valid(const nvm_addr* addrs, int num_addrs);
    void mark_sector_invalid(const nvm_addr& addr);
		struct nvm_addr **pa_table;
		private:
		int m_table_num;
		char *emb_table;
		
		
};






class cache2:public embedding_table_init2{
	public:
		
		void lru_page(vector<int> vectors, int table_ID);
		void transfer_vector(int &vec_ID, int &table_ID);
		float** get_emb_vectors(vector<int> &vectors, int table_ID);
		
		void GC_ondemand();
		void find_chunk(nvm_addr &addr, uint32_t &wp, int table_ID);
		cache2(int table_num, int size, int dimention):embedding_table_init2(table_num, size, dimention){
			hot_page_count = (0.01*dimention*size*sizeof(float)/byte_num)/get_table_num();
			count_sector = 0;
			emb_vec = new emb_vec_page*[get_table_num()];
			write_buf = new struct write_buffer[get_table_num()];

			page_count = 0;
			vector_table_offset = vector_page_offset*ws_opt;
			cache_size = 2;
			lru_main = new lrumain[get_table_num()];
			for (int i = 0 ; i <  get_table_num() ;i++){
				emb_vec[i] = new emb_vec_page[getsize_char() / geo->l.nbytes];
				write_buf[i].write_buf = new char [ws_opt * byte_num];
				//lru_main[i].hash_table.reserve(10000);
			}

			//lru_main.resize(table_num);


		};
		 //void write_chunk_statistics_to_csv(const std::string& filename);
		void read(vector<int> read_vectors, int table_ID);
		struct nvm_addr transfer_PA(int vec_ID, int table_ID);
		void Reclaim_vector_page(int table_ID);
		struct emb_vec_page **get_vec(){
			return emb_vec;
		}

		int hot_page_count;
		int cache_size;
		int page_count;
		int vector_table_offset;
		int get_pu(){
			return geo->l.npugrp;
		}
		void read_page(int i){
			// cout<<"channel "<<i/geo->l.npunit;
			// cout<<" PU "<<i%geo->l.npunit;
			// cout<<" read_page = "<<write_buf[i].read_page_count<<endl;
			write_buf[i].read_page_count = 0 ;
		}
		void write_page(int i){
			// cout<<"channel "<<i/geo->l.npunit;
			// cout<<" PU "<<i%geo->l.npunit;
			// cout<<" write_page = "<<write_buf[i].write_page_count<<endl;
			write_buf[i].write_page_count = 0 ;
		}
 
	
		struct lrumain *lru_main;
		nvm_addr GC_pa_table( int vec_start, nvm_addr gc_addr);
		nvm_addr GC_vectors(vector<int> vectors, int table_ID);
		void Reclaim_cold_table(int table_ID, int vector );
		struct write_buffer *write_buf;
	private:
		
		struct emb_vec_page **emb_vec;
		int count_sector;
		
};

#endif
