#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <liblightnvm.h>
#include <liblightnvm_spec.h>
#include <liblightnvm_cli.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include <list>
using namespace std;

struct emb_vec_page{
	struct nvm_addr update_addr;
	int16_t ID;
	float **data;
};

struct chunk_state{
	chunk_state(): invalid_page_num(0), sector(NULL){};
	int invalid_page_num;
	bool *sector;
	int sector_valid;     // 新增: 計數有效扇區
    int sector_invalid;   // 新增: 計數無效扇區
    vector<uint8_t> sector_states;  // 新增: 詳細的扇區狀態 (0=未使用, 1=有效, 2=無效)
};

struct write_buffer{
	write_buffer():read_page_count(0),write_page_count(0){};
	int read_page_count;
	int write_page_count;
};

struct lrumain{
	list<int> lru;
	map<int,decltype(lru.begin()) > hash_table;
};


class DATA{
	public:

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
		int getsize_char (){
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
			return m_table_num;
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
			chunkstate = new chunk_state*[geo->l.npugrp * geo->l.npunit];
			for (int i = 0 ; i <  geo->l.npugrp * geo->l.npunit ;i++){
				chunkstate[i] = new chunk_state[geo->l.nchunk];
				for (uint32_t j=0;j<geo->l.nchunk;j++){
					chunkstate[i][j].sector_states.resize(geo->l.nsectr,0);
				}
			}
			vector_page_offset = geo->l.nbytes / (get_dimention() * sizeof(float));
			chunk_used = 0;
			GC_threshold =0.1;
		};
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
		struct nvm_addr**pa_table;
		 void mark_sectors_valid(const nvm_addr* addrs, int num_addrs);
    void mark_sector_invalid(const nvm_addr& addr);
    void dump_chunk_sector_stats(const std::string &filename);
	private:
		int m_table_num;
		char *emb_table;
	
		
};





class embedding_table_init:protected DATA{
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
			return m_table_num;
		}
		int get_total_write_size(){
			return write_table * getsize_char();
		}
		char *get_emb_table_write(){
			return emb_table;
		}

		embedding_table_init(int table_num, int size, int dimention):m_table_num(table_num),DATA(size, dimention){
			
			dev = nvm_dev_open("/dev/nvme0n1");
			geo = nvm_dev_get_geo(dev);
			ws_opt = nvm_dev_get_ws_opt (dev);
			sector_num = geo->l.nsectr;
			byte_num = geo->l.nbytes;
			chunkstate = new chunk_state*[geo->l.npugrp * geo->l.npunit];
			for (int i = 0 ; i <  geo->l.npugrp * geo->l.npunit ;i++){
				chunkstate[i] = new chunk_state[geo->l.nchunk];
				for (uint32_t j=0;j<geo->l.nchunk;j++){
					chunkstate[i][j].sector_states.resize(geo->l.nsectr,0);
				}
			}
			vector_page_offset = geo->l.nbytes / (get_dimention() * sizeof(float));
			chunk_used = 0;
			GC_threshold =0.1;
		};

		void concat_emb();
		void write();
		~embedding_table_init(){
			//delete [] emb_table;
		}
		size_t ws_opt;
		int sector_num;
		int byte_num;
		struct nvm_dev *dev;
		const struct nvm_geo *geo;
		struct chunk_state **chunkstate;
		int vector_page_offset;
		int chunk_used;
		float GC_threshold;
		struct nvm_addr **pa_table;
		struct nvm_addr  *write_sync(char *write_buf,int byte);
		   void mark_sectors_valid(const nvm_addr* addrs, int num_addrs);
    void mark_sector_invalid(const nvm_addr& addr);
    void dump_chunk_sector_stats(const std::string &filename);
	private:
		int m_table_num;
		char *emb_table;
		//struct nvm_addr **pa_table;
		
};

class cache:public embedding_table_init{
	public:

		void lru_page(vector<int> &vectors, int table_ID);
		
		float** get_emb_vectors(vector<int> &vectors, int table_ID);
		void GC_ondemand();
		void find_chunk(nvm_addr &addr, uint32_t &wp);
		cache(int table_num, int size, int dimention):embedding_table_init(table_num, size, dimention){

			count_sector = 0;
			emb_vec = new emb_vec_page*[table_num];
			page_count = 0;
			write_buf = new struct write_buffer[get_table_num()];
			cache_size = 2;
			for (int i = 0 ; i <  get_table_num() ;i++){
				emb_vec[i] = new emb_vec_page[getsize_char() / geo->l.nbytes];
				write_buf[i].write_page_count = 0;
				write_buf[i].read_page_count = 0;
			}
			lru_main = new lrumain[get_table_num()];
			
		};
		void read(vector<int> &read_vectors, int table_ID);
		struct nvm_addr transfer_PA(int vec_ID, int table_ID);
		void Reclaim_vector_page(int table_ID);
		struct emb_vec_page **get_vec(){
			return emb_vec;
		}
		int get_pu(){
			return geo->l.npugrp;
		}
		void read_page(int i){
			//cout<<"channel "<<i/geo->l.npunit;
			//cout<<" PU "<<i%geo->l.npunit;
			//cout<<" read_page = "<<write_buf[i].read_page_count<<endl;
			write_buf[i].read_page_count = 0 ;
		}
		void write_page(int i){
			//cout<<"channel "<<i/geo->l.npunit;
			//cout<<" PU "<<i%geo->l.npunit;
			//cout<<" write_page = "<<write_buf[i].write_page_count<<endl;
			write_buf[i].write_page_count = 0 ;
		}
		//void write_chunk_statistics_to_csv(const std::string& filename);
               bool is_chunk_in_pa_table(int pu_id, int chunk_id);
		int cache_size;
		int page_count;
		struct write_buffer *write_buf;
		struct lrumain *lru_main;
	private:
		
		
		struct emb_vec_page **emb_vec;
		int count_sector;
		
};

