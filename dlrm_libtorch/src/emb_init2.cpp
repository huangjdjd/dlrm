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
#include <fstream>
#include "data2.h"
using namespace std;

void embedding_table_init2::concat_emb(){
	int j;
	int i;


	int opt_wt = ws_opt * byte_num;
	int wrt_size = get_total_write_size();


	if (wrt_size % opt_wt != 0){
		wrt_size += opt_wt - (get_total_write_size() % opt_wt);
	}


	int size = getsize_char();
	emb_table = new char[wrt_size];
	for ( i = 0; i < write_table; i++){
		char *emb;
		DATA emb_temp(get_size(),get_dimention());
		emb_temp.initial();
		emb_temp.transfer_byte();
		emb_temp.transfer_char1();


		//debug
		//emb_temp.transfer_char2();
		//emb_temp.transfer_float()[10000][0]<<endl;


		emb = emb_temp.get_char1();
		
		int k=0;
		for (j = i*emb_temp.getsize_char(); j < (i+1)*emb_temp.getsize_char();j++){
			emb_table[j] = emb[k];
			k++;
		}
		//strcat(emb_table, emb);



		// for (j = size*i; j < (i+1)*size;j++){
		// 	emb_table[j] = emb[j];
		// }

		delete [] emb;
	}
}
void embedding_table_init2::divide_emb(){
	int j;



	int opt_wt = ws_opt * byte_num;
	int wrt_size = get_total_write_size();


	if (wrt_size % opt_wt != 0){
		wrt_size += opt_wt - (get_total_write_size() % opt_wt);
	}

	DATA emb_temp(get_size()/write_table,get_dimention());
	emb_temp.initial();
	emb_temp.transfer_byte();


	emb_table = new char[wrt_size];
	
	char *emb;

	emb_temp.transfer_char1();



	emb = emb_temp.get_char1();
	
	int k=0;
	for (j = 0; j < emb_temp.getsize_char();j++){
		emb_table[j] = emb[k];
		k++;
	}


	delete [] emb;
	
	
	
	//padding
	for (j = get_total_write_size() ; j < wrt_size;j++){
			emb_table[j] = '0';
	}

}


void embedding_table_init2::find_free_chunk(nvm_addr &addr,int write_count){

    struct nvm_spec_rprt *rprt = NULL;

	


	rprt = NULL;

	for (uint32_t j = 0; j < geo->l.nchunk; j++){
		
		addr.l.pugrp = write_count/geo->l.npunit;
		addr.l.punit = write_count%geo->l.npunit;
		rprt = nvm_cmd_rprt(dev,&addr,0x0,NULL);
		if (rprt->descr[j].cs != NVM_CHUNK_STATE_FREE){
			continue;
		}
		addr.l.chunk = j;
		
		free_chunk--;
		return;
	}

    
}


struct nvm_addr *embedding_table_init2::write_sync(char *write_buf,int byte, int write_count){


	
	int err;
	

	int write_byte;
	int total_write = 0;
	struct nvm_addr *res_addr;
	int ppa_num =1 + byte / (geo->l.nsectr * geo->l.nbytes);
	res_addr = new nvm_addr[ppa_num ];
	int count = 0;
	while(1){
		
		struct nvm_addr chunk_addrs;
		chunk_addrs.val = 0;
		find_free_chunk(chunk_addrs, write_count);

		//printf("chunk= %d\n",chunk_addrs[0].l.chunk);


	
		const size_t ofz = count * geo->l.nsectr * geo->l.nbytes;
		res_addr[count] = chunk_addrs;
		count ++;
		for (size_t sectr = 0; sectr < geo->l.nsectr; sectr += ws_opt) {
			size_t buf_ofz = sectr * geo->l.nbytes + ofz;
			struct nvm_addr addrs[ws_opt];
			
			for (size_t aidx = 0; aidx < ws_opt; ++aidx) {
				addrs[aidx].val = chunk_addrs.val;
				addrs[aidx].l.sectr = sectr + aidx;
			}	


			if(byte <= 0){
				//cout<<"total_write = "<<total_write<<endl;
				return res_addr;
			}

			
			err = nvm_cmd_write(dev, addrs, ws_opt,
						write_buf + buf_ofz, NULL,
						0x0, NULL);

			if (err) {
				perror("nvm_cmd_write");
			}
			mark_sectors_valid(addrs, ws_opt);
		//	else{
		//		 mark_new_write(addrs, ws_opt);	
		//	}				

			write_byte =geo->l.nbytes*ws_opt;
			byte = byte - write_byte;
			total_write += write_byte;

		}
		

		
	}
	return res_addr;
}



void embedding_table_init2::write(){

	size_t nchunks = geo->l.npugrp * geo->l.npunit;
	const size_t write_size = nchunks*sector_num * byte_num;

	
	while (nchunks%get_table_num()!=0){
		nchunks--;
	}
	write_count = (getsize_char() / write_table ) / (sector_num * byte_num);
	
	if((getsize_char() / write_table ) % (sector_num * byte_num) !=0){
		write_count++;
	}

	pa_table = new nvm_addr *[nchunks];



	for (int i = 0 ;i < nchunks; i++){
		pa_table[i] = new nvm_addr[write_count];
		
		divide_emb();

		struct nvm_addr *res2 = write_sync(get_emb_table_write(), get_total_write_size(), i);
		for(int j = 0; j < write_count; j++){

			pa_table[i][j] = res2[j];
			cout<<"\t"<<"pa["<<i<<"]"<<"["<<j<<"].l.chunk="<<pa_table[i][j].l.chunk<<";"<<endl;
			
			chunk_used++;
			
			chunkstate[pa_table[i][j].l.pugrp * geo->l.npunit + pa_table[i][j].l.punit][pa_table[i][j].l.chunk].invalid_vector_num = 0;
			
		}
		
		delete [] emb_table;
	}



}
// 在 embedding_table_init2 類中添加這個函數
void embedding_table_init2::mark_sectors_valid(const nvm_addr* addrs, int num_addrs) {
    for (int i = 0; i < num_addrs; i++) {
        int pu_idx = addrs[i].l.pugrp * geo->l.npunit + addrs[i].l.punit;
        int chunk_idx = addrs[i].l.chunk;
        int sectr_idx = addrs[i].l.sectr;

        // 如果扇區之前是未使用的，標記為有效並增加計數
        if (chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] == 0) {
            chunkstate[pu_idx][chunk_idx].sector_valid++;
            chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] = 1; // 標記為有效
        }
        // 如果扇區之前是無效的，轉換為有效並更新計數
        else if (chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] == 2) {
            chunkstate[pu_idx][chunk_idx].sector_invalid--;
            chunkstate[pu_idx][chunk_idx].sector_valid++;
            chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] = 1;
        }
    }
}

void embedding_table_init2::mark_sector_invalid(const nvm_addr& addr) {
    int pu_idx = addr.l.pugrp * geo->l.npunit + addr.l.punit;
    int chunk_idx = addr.l.chunk;
    int sectr_idx = addr.l.sectr;

    // 如果扇區當前是有效的，將其標記為無效
    if (chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] == 1) {
        chunkstate[pu_idx][chunk_idx].sector_valid--;
        chunkstate[pu_idx][chunk_idx].sector_invalid++;
        chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] = 2; // 標記為無效
    }
}
void embedding_table_init2::dump_chunk_sector_stats(const std::string &filename) {
    std::ofstream csvfile(filename);
    csvfile << "PuGrp,PUnit,Chunk,ValidSectors,InvalidSectors,UnusedSectors,ValidRatio,InvalidRatio\n";
    
    for (int i = 0; i < geo->l.npugrp * geo->l.npunit; i++) {
        for (int j = 0; j < geo->l.nchunk; j++) {
            // 只輸出有寫入的 chunk
            int valid = chunkstate[i][j].sector_valid;
            int invalid = chunkstate[i][j].sector_invalid;
            
            if (valid > 0 || invalid > 0) {
                int unused = geo->l.nsectr - valid - invalid;
                float valid_ratio = (float)valid / geo->l.nsectr;
                float invalid_ratio = (float)invalid / geo->l.nsectr;
                
                csvfile << i / geo->l.npunit << "," // PuGrp
                        << i % geo->l.npunit << "," // PUnit
                        << j << "," // Chunk
                        << valid << "," 
                        << invalid << "," 
                        << unused << "," 
                        << valid_ratio << "," 
                        << invalid_ratio << "\n";
            }
        }
    }
    csvfile.close();
}
void embedding_table_init2::pa_init(){
	size_t nchunks = geo->l.npugrp * geo->l.npunit;


	while (nchunks%get_table_num()!=0){
		nchunks--;
	}
	write_count = (getsize_char() / write_table ) / (sector_num * byte_num);
	
	if((getsize_char() / write_table ) % (sector_num * byte_num) !=0){
		write_count++;
	}

	struct nvm_addr **pa;
	pa = new nvm_addr *[nchunks];
	for (int i = 0; i < nchunks; i++){
		pa[i] = new nvm_addr[write_count];
		for(int j = 0; j < write_count;j++){
			pa[i][j].l.pugrp = i / geo->l.npunit;
			pa[i][j].l.punit = i % geo->l.npunit;
			chunk_used++;
		}
	} 
/*pa[0][0].l.chunk=0;
	pa[0][1].l.chunk=1;
	pa[0][2].l.chunk=2;
	pa[0][3].l.chunk=3;
	pa[0][4].l.chunk=4;
	pa[0][5].l.chunk=5;
	pa[0][6].l.chunk=6;
	pa[0][7].l.chunk=7;
	pa[1][0].l.chunk=0;
	pa[1][1].l.chunk=1;
	pa[1][2].l.chunk=2;
	pa[1][3].l.chunk=3;
	pa[1][4].l.chunk=4;
	pa[1][5].l.chunk=5;
	pa[1][6].l.chunk=6;
	pa[1][7].l.chunk=7;
	pa[2][0].l.chunk=0;
	pa[2][1].l.chunk=1;
	pa[2][2].l.chunk=2;
	pa[2][3].l.chunk=3;
	pa[2][4].l.chunk=4;
	pa[2][5].l.chunk=5;
	pa[2][6].l.chunk=6;
	pa[2][7].l.chunk=7;
	pa[3][0].l.chunk=0;
	pa[3][1].l.chunk=1;
	pa[3][2].l.chunk=2;
	pa[3][3].l.chunk=3;
	pa[3][4].l.chunk=4;
	pa[3][5].l.chunk=5;
	pa[3][6].l.chunk=6;
	pa[3][7].l.chunk=7;
	pa[4][0].l.chunk=0;
	pa[4][1].l.chunk=1;
	pa[4][2].l.chunk=2;
	pa[4][3].l.chunk=3;
	pa[4][4].l.chunk=4;
	pa[4][5].l.chunk=5;
	pa[4][6].l.chunk=6;
	pa[4][7].l.chunk=7;
	pa[5][0].l.chunk=0;
	pa[5][1].l.chunk=1;
	pa[5][2].l.chunk=2;
	pa[5][3].l.chunk=3;
	pa[5][4].l.chunk=4;
	pa[5][5].l.chunk=5;
	pa[5][6].l.chunk=6;
	pa[5][7].l.chunk=7;
	pa[6][0].l.chunk=0;
	pa[6][1].l.chunk=1;
	pa[6][2].l.chunk=2;
	pa[6][3].l.chunk=3;
	pa[6][4].l.chunk=4;
	pa[6][5].l.chunk=5;
	pa[6][6].l.chunk=6;
	pa[6][7].l.chunk=7;
	pa[7][0].l.chunk=0;
	pa[7][1].l.chunk=1;
	pa[7][2].l.chunk=2;
	pa[7][3].l.chunk=3;
	pa[7][4].l.chunk=4;
	pa[7][5].l.chunk=5;
	pa[7][6].l.chunk=6;
	pa[7][7].l.chunk=7;
	pa[8][0].l.chunk=0;
	pa[8][1].l.chunk=1;
	pa[8][2].l.chunk=2;
	pa[8][3].l.chunk=3;
	pa[8][4].l.chunk=4;
	pa[8][5].l.chunk=5;
	pa[8][6].l.chunk=6;
	pa[8][7].l.chunk=7;
	pa[9][0].l.chunk=0;
	pa[9][1].l.chunk=1;
	pa[9][2].l.chunk=2;
	pa[9][3].l.chunk=3;
	pa[9][4].l.chunk=4;
	pa[9][5].l.chunk=5;
	pa[9][6].l.chunk=6;
	pa[9][7].l.chunk=7;
	pa[10][0].l.chunk=0;
	pa[10][1].l.chunk=1;
	pa[10][2].l.chunk=2;
	pa[10][3].l.chunk=3;
	pa[10][4].l.chunk=4;
	pa[10][5].l.chunk=5;
	pa[10][6].l.chunk=6;
	pa[10][7].l.chunk=7;
	pa[11][0].l.chunk=0;
	pa[11][1].l.chunk=1;
	pa[11][2].l.chunk=2;
	pa[11][3].l.chunk=3;
	pa[11][4].l.chunk=4;
	pa[11][5].l.chunk=5;
	pa[11][6].l.chunk=6;
	pa[11][7].l.chunk=7;
	pa[12][0].l.chunk=0;
	pa[12][1].l.chunk=1;
	pa[12][2].l.chunk=2;
	pa[12][3].l.chunk=3;
	pa[12][4].l.chunk=4;
	pa[12][5].l.chunk=5;
	pa[12][6].l.chunk=6;
	pa[12][7].l.chunk=7;
	pa[13][0].l.chunk=0;
	pa[13][1].l.chunk=1;
	pa[13][2].l.chunk=2;
	pa[13][3].l.chunk=3;
	pa[13][4].l.chunk=4;
	pa[13][5].l.chunk=5;
	pa[13][6].l.chunk=6;
	pa[13][7].l.chunk=7;
	pa[14][0].l.chunk=0;
	pa[14][1].l.chunk=1;
	pa[14][2].l.chunk=2;
	pa[14][3].l.chunk=3;
	pa[14][4].l.chunk=4;
	pa[14][5].l.chunk=5;
	pa[14][6].l.chunk=6;
	pa[14][7].l.chunk=7;
	pa[15][0].l.chunk=0;
	pa[15][1].l.chunk=1;
	pa[15][2].l.chunk=2;
	pa[15][3].l.chunk=3;
	pa[15][4].l.chunk=4;
	pa[15][5].l.chunk=5;
	pa[15][6].l.chunk=6;
	pa[15][7].l.chunk=7;
	pa[16][0].l.chunk=0;
	pa[16][1].l.chunk=1;
	pa[16][2].l.chunk=2;
	pa[16][3].l.chunk=3;
	pa[16][4].l.chunk=4;
	pa[16][5].l.chunk=5;
	pa[16][6].l.chunk=6;
	pa[16][7].l.chunk=7;
	pa[17][0].l.chunk=0;
	pa[17][1].l.chunk=1;
	pa[17][2].l.chunk=2;
	pa[17][3].l.chunk=3;
	pa[17][4].l.chunk=4;
	pa[17][5].l.chunk=5;
	pa[17][6].l.chunk=6;
	pa[17][7].l.chunk=7;
	pa[18][0].l.chunk=0;
	pa[18][1].l.chunk=1;
	pa[18][2].l.chunk=2;
	pa[18][3].l.chunk=3;
	pa[18][4].l.chunk=4;
	pa[18][5].l.chunk=5;
	pa[18][6].l.chunk=6;
	pa[18][7].l.chunk=7;
	pa[19][0].l.chunk=0;
	pa[19][1].l.chunk=1;
	pa[19][2].l.chunk=2;
	pa[19][3].l.chunk=3;
	pa[19][4].l.chunk=4;
	pa[19][5].l.chunk=5;
	pa[19][6].l.chunk=6;
	pa[19][7].l.chunk=7;
	pa[20][0].l.chunk=0;
	pa[20][1].l.chunk=1;
	pa[20][2].l.chunk=2;
	pa[20][3].l.chunk=3;
	pa[20][4].l.chunk=4;
	pa[20][5].l.chunk=5;
	pa[20][6].l.chunk=6;
	pa[20][7].l.chunk=7;
	pa[21][0].l.chunk=0;
	pa[21][1].l.chunk=1;
	pa[21][2].l.chunk=2;
	pa[21][3].l.chunk=3;
	pa[21][4].l.chunk=4;
	pa[21][5].l.chunk=5;
	pa[21][6].l.chunk=6;
	pa[21][7].l.chunk=7;
	pa[22][0].l.chunk=0;
	pa[22][1].l.chunk=1;
	pa[22][2].l.chunk=2;
	pa[22][3].l.chunk=3;
	pa[22][4].l.chunk=4;
	pa[22][5].l.chunk=5;
	pa[22][6].l.chunk=6;
	pa[22][7].l.chunk=7;
	pa[23][0].l.chunk=0;
	pa[23][1].l.chunk=1;
	pa[23][2].l.chunk=2;
	pa[23][3].l.chunk=3;
	pa[23][4].l.chunk=4;
	pa[23][5].l.chunk=5;
	pa[23][6].l.chunk=6;
	pa[23][7].l.chunk=7;
	pa[24][0].l.chunk=0;
	pa[24][1].l.chunk=1;
	pa[24][2].l.chunk=2;
	pa[24][3].l.chunk=3;
	pa[24][4].l.chunk=4;
	pa[24][5].l.chunk=5;
	pa[24][6].l.chunk=6;
	pa[24][7].l.chunk=7;
	pa[25][0].l.chunk=0;
	pa[25][1].l.chunk=1;
	pa[25][2].l.chunk=2;
	pa[25][3].l.chunk=3;
	pa[25][4].l.chunk=4;
	pa[25][5].l.chunk=5;
	pa[25][6].l.chunk=6;
	pa[25][7].l.chunk=7;
	pa[26][0].l.chunk=0;
	pa[26][1].l.chunk=1;
	pa[26][2].l.chunk=2;
	pa[26][3].l.chunk=3;
	pa[26][4].l.chunk=4;
	pa[26][5].l.chunk=5;
	pa[26][6].l.chunk=6;
	pa[26][7].l.chunk=7;
	pa[27][0].l.chunk=0;
	pa[27][1].l.chunk=1;
	pa[27][2].l.chunk=2;
	pa[27][3].l.chunk=3;
	pa[27][4].l.chunk=4;
	pa[27][5].l.chunk=5;
	pa[27][6].l.chunk=6;
	pa[27][7].l.chunk=7;
	pa[28][0].l.chunk=0;
	pa[28][1].l.chunk=1;
	pa[28][2].l.chunk=2;
	pa[28][3].l.chunk=3;
	pa[28][4].l.chunk=4;
	pa[28][5].l.chunk=5;
	pa[28][6].l.chunk=6;
	pa[28][7].l.chunk=7;
	pa[29][0].l.chunk=0;
	pa[29][1].l.chunk=1;
	pa[29][2].l.chunk=2;
	pa[29][3].l.chunk=3;
	pa[29][4].l.chunk=4;
	pa[29][5].l.chunk=5;
	pa[29][6].l.chunk=6;
	pa[29][7].l.chunk=7;
	pa[30][0].l.chunk=0;
	pa[30][1].l.chunk=1;
	pa[30][2].l.chunk=2;
	pa[30][3].l.chunk=3;
	pa[30][4].l.chunk=4;
	pa[30][5].l.chunk=5;
	pa[30][6].l.chunk=6;
	pa[30][7].l.chunk=7;
	pa[31][0].l.chunk=0;
	pa[31][1].l.chunk=1;
	pa[31][2].l.chunk=2;
	pa[31][3].l.chunk=3;
	pa[31][4].l.chunk=4;
	pa[31][5].l.chunk=5;
	pa[31][6].l.chunk=6;
	pa[31][7].l.chunk=7;*/
	     pa[0][0].l.chunk=1;
        pa[0][1].l.chunk=2;
        pa[0][2].l.chunk=3;
        pa[0][3].l.chunk=4;
        pa[0][4].l.chunk=5;
        pa[0][5].l.chunk=6;
        pa[1][0].l.chunk=0;
        pa[1][1].l.chunk=1;
        pa[1][2].l.chunk=2;
        pa[1][3].l.chunk=3;
        pa[1][4].l.chunk=4;
        pa[1][5].l.chunk=5;
        pa[2][0].l.chunk=0;
        pa[2][1].l.chunk=1;
        pa[2][2].l.chunk=2;
        pa[2][3].l.chunk=3;
        pa[2][4].l.chunk=4;
        pa[2][5].l.chunk=5;
        pa[3][0].l.chunk=0;
        pa[3][1].l.chunk=1;
        pa[3][2].l.chunk=2;
        pa[3][3].l.chunk=3;
        pa[3][4].l.chunk=4;
        pa[3][5].l.chunk=5;
        pa[4][0].l.chunk=0;
        pa[4][1].l.chunk=1;
        pa[4][2].l.chunk=2;
        pa[4][3].l.chunk=3;
        pa[4][4].l.chunk=4;
        pa[4][5].l.chunk=5;
        pa[5][0].l.chunk=0;
        pa[5][1].l.chunk=1;
        pa[5][2].l.chunk=2;
        pa[5][3].l.chunk=3;
        pa[5][4].l.chunk=4;
        pa[5][5].l.chunk=5;
        pa[6][0].l.chunk=0;
        pa[6][1].l.chunk=1;
        pa[6][2].l.chunk=2;
        pa[6][3].l.chunk=3;
        pa[6][4].l.chunk=4;
        pa[6][5].l.chunk=5;
        pa[7][0].l.chunk=0;
        pa[7][1].l.chunk=1;
        pa[7][2].l.chunk=2;
        pa[7][3].l.chunk=3;
        pa[7][4].l.chunk=4;
        pa[7][5].l.chunk=5;
        pa[8][0].l.chunk=0;
        pa[8][1].l.chunk=1;
        pa[8][2].l.chunk=2;
        pa[8][3].l.chunk=3;
        pa[8][4].l.chunk=4;
        pa[8][5].l.chunk=5;
        pa[9][0].l.chunk=0;
        pa[9][1].l.chunk=1;
        pa[9][2].l.chunk=2;
        pa[9][3].l.chunk=3;
        pa[9][4].l.chunk=4;
        pa[9][5].l.chunk=5;
        pa[10][0].l.chunk=0;
        pa[10][1].l.chunk=1;
        pa[10][2].l.chunk=2;
        pa[10][3].l.chunk=3;
        pa[10][4].l.chunk=4;
        pa[10][5].l.chunk=5;
        pa[11][0].l.chunk=0;
        pa[11][1].l.chunk=1;
        pa[11][2].l.chunk=2;
        pa[11][3].l.chunk=3;
        pa[11][4].l.chunk=4;
        pa[11][5].l.chunk=5;
        pa[12][0].l.chunk=0;
        pa[12][1].l.chunk=1;
        pa[12][2].l.chunk=2;
        pa[12][3].l.chunk=3;
        pa[12][4].l.chunk=4;
        pa[12][5].l.chunk=5;
        pa[13][0].l.chunk=0;
        pa[13][1].l.chunk=1;
        pa[13][2].l.chunk=2;
        pa[13][3].l.chunk=3;
        pa[13][4].l.chunk=4;
        pa[13][5].l.chunk=5;
        pa[14][0].l.chunk=0;
        pa[14][1].l.chunk=1;
        pa[14][2].l.chunk=2;
        pa[14][3].l.chunk=3;
        pa[14][4].l.chunk=4;
        pa[14][5].l.chunk=5;
        pa[15][0].l.chunk=0;
        pa[15][1].l.chunk=1;
        pa[15][2].l.chunk=2;
        pa[15][3].l.chunk=3;
        pa[15][4].l.chunk=4;
        pa[15][5].l.chunk=5;
        pa[16][0].l.chunk=0;
        pa[16][1].l.chunk=1;
        pa[16][2].l.chunk=2;
        pa[16][3].l.chunk=3;
        pa[16][4].l.chunk=4;
        pa[16][5].l.chunk=5;
        pa[17][0].l.chunk=0;
        pa[17][1].l.chunk=1;
        pa[17][2].l.chunk=2;
        pa[17][3].l.chunk=3;
        pa[17][4].l.chunk=4;
        pa[17][5].l.chunk=5;
        pa[18][0].l.chunk=0;
        pa[18][1].l.chunk=1;
        pa[18][2].l.chunk=2;
        pa[18][3].l.chunk=3;
        pa[18][4].l.chunk=4;
        pa[18][5].l.chunk=5;
        pa[19][0].l.chunk=0;
        pa[19][1].l.chunk=1;
        pa[19][2].l.chunk=2;
        pa[19][3].l.chunk=3;
        pa[19][4].l.chunk=4;
        pa[19][5].l.chunk=5;
        pa[20][0].l.chunk=0;
        pa[20][1].l.chunk=1;
        pa[20][2].l.chunk=2;
        pa[20][3].l.chunk=3;
        pa[20][4].l.chunk=4;
        pa[20][5].l.chunk=5;
        pa[21][0].l.chunk=0;
        pa[21][1].l.chunk=1;
        pa[21][2].l.chunk=2;
        pa[21][3].l.chunk=3;
        pa[21][4].l.chunk=4;
        pa[21][5].l.chunk=5;
        pa[22][0].l.chunk=0;
        pa[22][1].l.chunk=1;
        pa[22][2].l.chunk=2;
        pa[22][3].l.chunk=3;
        pa[22][4].l.chunk=4;
        pa[22][5].l.chunk=5;
        pa[23][0].l.chunk=0;
        pa[23][1].l.chunk=1;
        pa[23][2].l.chunk=2;
        pa[23][3].l.chunk=3;
        pa[23][4].l.chunk=4;
        pa[23][5].l.chunk=5;
        pa[24][0].l.chunk=0;
        pa[24][1].l.chunk=1;
        pa[24][2].l.chunk=2;
        pa[24][3].l.chunk=3;
        pa[24][4].l.chunk=4;
        pa[24][5].l.chunk=5;
        pa[25][0].l.chunk=0;
        pa[25][1].l.chunk=1;
        pa[25][2].l.chunk=2;
        pa[25][3].l.chunk=3;
        pa[25][4].l.chunk=4;
        pa[25][5].l.chunk=5;
        pa[26][0].l.chunk=0;
        pa[26][1].l.chunk=1;
        pa[26][2].l.chunk=2;
        pa[26][3].l.chunk=3;
        pa[26][4].l.chunk=4;
        pa[26][5].l.chunk=5;
        pa[27][0].l.chunk=0;
        pa[27][1].l.chunk=1;
        pa[27][2].l.chunk=2;
        pa[27][3].l.chunk=3;
        pa[27][4].l.chunk=4;
        pa[27][5].l.chunk=5;
        pa[28][0].l.chunk=0;
        pa[28][1].l.chunk=1;
        pa[28][2].l.chunk=2;
        pa[28][3].l.chunk=3;
        pa[28][4].l.chunk=4;
        pa[28][5].l.chunk=5;
        pa[29][0].l.chunk=0;
        pa[29][1].l.chunk=1;
        pa[29][2].l.chunk=2;
        pa[29][3].l.chunk=3;
        pa[29][4].l.chunk=4;
        pa[29][5].l.chunk=5;


	pa_table = pa; 
	
	for (int i = 0; i < nchunks; i++){

		for(int j = 0; j < write_count;j++){
			chunkstate[pa[i][j].l.pugrp * geo->l.npunit + pa[i][j].l.punit][pa[i][j].l.chunk].invalid_vector_num = 0;
		}
		
	} 
	init_free_page();
	


	

}

void embedding_table_init2::init_free_page() {
	
}




struct nvm_addr embedding_table_init2::transfer_PA_from_init(int vec_ID, int table_ID){

	struct nvm_addr **pa = get_pa_table();


	
	int	table_county = vec_ID*get_dimention()*sizeof(float)/(sector_num*byte_num);
	int sec_addr = (vec_ID/vector_page_offset) % sector_num;

	struct nvm_addr res;
	res = pa[table_ID][table_county];
	res.l.sectr = sec_addr;

	return res;
}

// int main(){
//     embedding_table_init2 emb2(32,500000,64);
// 	emb2.write();
// 	// struct nvm_addr **pa = emb2.get_pa_table();
// 	//emb2.pa_init();
// 	struct nvm_addr test = emb2.transfer_PA_from_init(10000, 0);
	
// }

