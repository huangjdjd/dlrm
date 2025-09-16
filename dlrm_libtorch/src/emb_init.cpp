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
#include <fstream>
using namespace std;



void embedding_table_init::concat_emb(){
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
	
	
	//padding
	for (j = get_total_write_size() ; j < wrt_size;j++){
			emb_table[j] = '0';
	}

}





struct nvm_addr *embedding_table_init::write_sync(char *write_buf,int byte){
	struct nvm_dev *dev;
	const struct nvm_geo *geo;

	dev = nvm_dev_open("/dev/nvme0n1");
	geo = nvm_dev_get_geo(dev);

	const size_t nchunks = geo->l.npugrp * geo->l.npunit;

	size_t ws_opt = nvm_dev_get_ws_opt(dev);
	int err;
	
	const size_t write_size = nchunks*geo->l.nsectr * geo->l.nbytes;
	int write_byte;
	int total_write = 0;
	struct nvm_addr *res_addr;
	int ppa_num =1 + byte / (geo->l.nsectr * geo->l.nbytes);
	res_addr = new nvm_addr[ppa_num ];
	int count = 0;
	while(1){
		
		struct nvm_addr chunk_addrs[nchunks];
		err = nvm_cmd_rprt_arbs(dev, NVM_CHUNK_STATE_FREE, nchunks,
					chunk_addrs);
		if (err) {
			perror("nvm_cmd_rprt_arbs");
		}

		//printf("chunk= %d\n",chunk_addrs[0].l.chunk);


		for (size_t cidx = 0; cidx < nchunks; ++cidx) {
			const size_t ofz = cidx * geo->l.nsectr * geo->l.nbytes;
			res_addr[count] = chunk_addrs[cidx];
			count ++;
			for (size_t sectr = 0; sectr < geo->l.nsectr; sectr += ws_opt) {
				size_t buf_ofz = sectr * geo->l.nbytes + ofz;
				struct nvm_addr addrs[ws_opt];
				
				for (size_t aidx = 0; aidx < ws_opt; ++aidx) {
					addrs[aidx].val = chunk_addrs[cidx].val;
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

				write_byte =geo->l.nbytes*ws_opt;
				byte = byte - write_byte;
				total_write += write_byte;

			}
		}

		
	}
	return res_addr;
}


void embedding_table_init::write(){
	struct nvm_dev *dev;
	const struct nvm_geo *geo;
	dev = nvm_dev_open("/dev/nvme0n1");
	geo = nvm_dev_get_geo(dev);
	const size_t nchunks = geo->l.npugrp * geo->l.npunit;
	const size_t write_size = nchunks*sector_num * byte_num;
	write_table = write_size/getsize_char();
	write_count = get_table_num()/write_table;
	int write_pa_count = get_total_write_size() / (sector_num * byte_num);
	if(get_total_write_size() % (sector_num * byte_num) !=0){
		write_pa_count++;
	}

	pa_table = new nvm_addr *[write_count];



	for (int i = 0 ;i < write_count; i++){
		pa_table[i] = new nvm_addr[write_pa_count];
		
		concat_emb();

		struct nvm_addr *res2 = write_sync(get_emb_table_write(), get_total_write_size());

		for(int j = 0; j < write_pa_count; j++){
			pa_table[i][j] = res2[j];
			cout<<"\t"<<"pa["<<i<<"]"<<"["<<j<<"].l.pugrp="<<pa_table[i][j].l.pugrp<<";"<<endl;
			cout<<"\t"<<"pa["<<i<<"]"<<"["<<j<<"].l.punit="<<pa_table[i][j].l.punit<<";"<<endl;
			chunk_used++;
			
			chunkstate[pa_table[i][j].l.pugrp * geo->l.npunit + pa_table[i][j].l.punit][pa_table[i][j].l.chunk].invalid_page_num = 0;
			chunkstate[pa_table[i][j].l.pugrp * geo->l.npunit + pa_table[i][j].l.punit][pa_table[i][j].l.chunk].sector = new bool[sector_num];
			for (int k = 0; k < sector_num; k++){
				chunkstate[pa_table[i][j].l.pugrp * geo->l.npunit + pa_table[i][j].l.punit][pa_table[i][j].l.chunk].sector[k] = 1;
			}

		}
		
		delete [] emb_table;
	}

	for (int i = 0 ;i < write_count; i++){
		cout<<"pa["<<i<<"]"<<"[0]"<<"l.pugrp="<<pa_table[i][0].l.chunk<<endl;
	}



}
// 標記扇區為有效
void embedding_table_init::mark_sectors_valid(const nvm_addr* addrs, int num_addrs) {
    for (int i = 0; i < num_addrs; i++) {
        int pu_idx = addrs[i].l.pugrp * geo->l.npunit + addrs[i].l.punit;
        int chunk_idx = addrs[i].l.chunk;
        int sectr_idx = addrs[i].l.sectr;
        
        // 檢查 sector_states 是否已初始化
        if (chunkstate[pu_idx][chunk_idx].sector_states.empty()) {
            chunkstate[pu_idx][chunk_idx].sector_states.resize(sector_num, 0);
        }
        
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

// 標記扇區為無效
void embedding_table_init::mark_sector_invalid(const nvm_addr& addr) {
    int pu_idx = addr.l.pugrp * geo->l.npunit + addr.l.punit;
    int chunk_idx = addr.l.chunk;
    int sectr_idx = addr.l.sectr;
    
    // 確保 sector_states 已初始化
    if (chunkstate[pu_idx][chunk_idx].sector_states.empty()) {
        chunkstate[pu_idx][chunk_idx].sector_states.resize(sector_num, 0);
    }
    
    // 如果扇區當前是有效的，將其標記為無效
    if (chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] == 1) {
        chunkstate[pu_idx][chunk_idx].sector_valid--;
        chunkstate[pu_idx][chunk_idx].sector_invalid++;
        chunkstate[pu_idx][chunk_idx].sector_states[sectr_idx] = 2; // 標記為無效
    }
}

// 輸出統計資料到CSV
void embedding_table_init::dump_chunk_sector_stats(const std::string &filename) {
    std::ofstream csvfile(filename);
    if (!csvfile.is_open()) {
        std::cerr << "無法開啟檔案: " << filename << std::endl;
        return;
    }
    
    csvfile << "PuGrp,PUnit,Chunk,ValidSectors,InvalidSectors,UnusedSectors,ValidRatio,InvalidRatio\n";
    
    for (int i = 0; i < geo->l.npugrp * geo->l.npunit; i++) {
        for (int j = 0; j < geo->l.nchunk; j++) {
            // 如果 sector_states 未初始化，跳過
            if (chunkstate[i][j].sector_states.empty()) {
                continue;
            }
            
            int valid = chunkstate[i][j].sector_valid;
            int invalid = chunkstate[i][j].sector_invalid;
            
            // 只輸出有寫入的 chunk
            if (valid > 0 || invalid > 0) {
                int unused = sector_num - valid - invalid;
                float valid_ratio = (float)valid / sector_num;
                float invalid_ratio = (float)invalid / sector_num;
                
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
void embedding_table_init::pa_init(){
	write_table = (geo->l.npugrp * geo->l.npunit * geo->l.nsectr * geo->l.nbytes)/getsize_char();
	write_count = get_table_num()/write_table;
	struct nvm_addr **pa;
	pa = new nvm_addr *[8];
	for (int i = 0; i < 8; i++){
		pa[i] = new nvm_addr[31];
		for(int j = 0; j < 31;j++){
			if(i==0) pa[i][j].l.chunk = 23;
			if(i==1) pa[i][j].l.chunk = 27;
			if(i==2) pa[i][j].l.chunk = 17;
			if(i==3) pa[i][j].l.chunk = 15;
			if(i==4) pa[i][j].l.chunk = 28;
			if(i==5) pa[i][j].l.chunk = 29;
			if(i==6) pa[i][j].l.chunk = 24;
			if(i==7) pa[i][j].l.chunk = 16;
			chunk_used++;
		}
	} 
	pa[0][0].l.pugrp=3;
	pa[0][0].l.punit=3;
	pa[0][1].l.pugrp=4;
	pa[0][1].l.punit=3;
	pa[0][2].l.pugrp=5;
	pa[0][2].l.punit=3;
	pa[0][3].l.pugrp=6;
	pa[0][3].l.punit=3;
	pa[0][4].l.pugrp=7;
	pa[0][4].l.punit=3;
	pa[0][5].l.pugrp=0;
	pa[0][5].l.punit=0;
	pa[0][6].l.pugrp=1;
	pa[0][6].l.punit=0;
	pa[0][7].l.pugrp=2;
	pa[0][7].l.punit=0;
	pa[0][8].l.pugrp=3;
	pa[0][8].l.punit=0;
	pa[0][9].l.pugrp=4;
	pa[0][9].l.punit=0;
	pa[0][10].l.pugrp=5;
	pa[0][10].l.punit=0;
	pa[0][11].l.pugrp=6;
	pa[0][11].l.punit=0;
	pa[0][12].l.pugrp=7;
	pa[0][12].l.punit=0;
	pa[0][13].l.pugrp=0;
	pa[0][13].l.punit=1;
	pa[0][14].l.pugrp=1;
	pa[0][14].l.punit=1;
	pa[0][15].l.pugrp=2;
	pa[0][15].l.punit=1;
	pa[0][16].l.pugrp=3;
	pa[0][16].l.punit=1;
	pa[0][17].l.pugrp=4;
	pa[0][17].l.punit=1;
	pa[0][18].l.pugrp=5;
	pa[0][18].l.punit=1;
	pa[0][19].l.pugrp=6;
	pa[0][19].l.punit=1;
	pa[0][20].l.pugrp=7;
	pa[0][20].l.punit=1;
	pa[0][21].l.pugrp=0;
	pa[0][21].l.punit=2;
	pa[0][22].l.pugrp=1;
	pa[0][22].l.punit=2;
	pa[0][23].l.pugrp=2;
	pa[0][23].l.punit=2;
	pa[0][24].l.pugrp=3;
	pa[0][24].l.punit=2;
	pa[0][25].l.pugrp=4;
	pa[0][25].l.punit=2;
	pa[0][26].l.pugrp=5;
	pa[0][26].l.punit=2;
	pa[0][27].l.pugrp=6;
	pa[0][27].l.punit=2;
	pa[0][28].l.pugrp=7;
	pa[0][28].l.punit=2;
	pa[0][29].l.pugrp=0;
	pa[0][29].l.punit=3;
	pa[0][30].l.pugrp=1;
	pa[0][30].l.punit=3;
	pa[1][0].l.pugrp=1;
	pa[1][0].l.punit=3;
	pa[1][1].l.pugrp=2;
	pa[1][1].l.punit=3;
	pa[1][2].l.pugrp=3;
	pa[1][2].l.punit=3;
	pa[1][3].l.pugrp=4;
	pa[1][3].l.punit=3;
	pa[1][4].l.pugrp=5;
	pa[1][4].l.punit=3;
	pa[1][5].l.pugrp=6;
	pa[1][5].l.punit=3;
	pa[1][6].l.pugrp=7;
	pa[1][6].l.punit=3;
	pa[1][7].l.pugrp=0;
	pa[1][7].l.punit=0;
	pa[1][8].l.pugrp=1;
	pa[1][8].l.punit=0;
	pa[1][9].l.pugrp=2;
	pa[1][9].l.punit=0;
	pa[1][10].l.pugrp=3;
	pa[1][10].l.punit=0;
	pa[1][11].l.pugrp=4;
	pa[1][11].l.punit=0;
	pa[1][12].l.pugrp=5;
	pa[1][12].l.punit=0;
	pa[1][13].l.pugrp=6;
	pa[1][13].l.punit=0;
	pa[1][14].l.pugrp=7;
	pa[1][14].l.punit=0;
	pa[1][15].l.pugrp=0;
	pa[1][15].l.punit=1;
	pa[1][16].l.pugrp=1;
	pa[1][16].l.punit=1;
	pa[1][17].l.pugrp=2;
	pa[1][17].l.punit=1;
	pa[1][18].l.pugrp=3;
	pa[1][18].l.punit=1;
	pa[1][19].l.pugrp=4;
	pa[1][19].l.punit=1;
	pa[1][20].l.pugrp=5;
	pa[1][20].l.punit=1;
	pa[1][21].l.pugrp=6;
	pa[1][21].l.punit=1;
	pa[1][22].l.pugrp=7;
	pa[1][22].l.punit=1;
	pa[1][23].l.pugrp=0;
	pa[1][23].l.punit=2;
	pa[1][24].l.pugrp=1;
	pa[1][24].l.punit=2;
	pa[1][25].l.pugrp=2;
	pa[1][25].l.punit=2;
	pa[1][26].l.pugrp=3;
	pa[1][26].l.punit=2;
	pa[1][27].l.pugrp=4;
	pa[1][27].l.punit=2;
	pa[1][28].l.pugrp=5;
	pa[1][28].l.punit=2;
	pa[1][29].l.pugrp=6;
	pa[1][29].l.punit=2;
	pa[1][30].l.pugrp=7;
	pa[1][30].l.punit=2;
	pa[2][0].l.pugrp=1;
	pa[2][0].l.punit=3;
	pa[2][1].l.pugrp=2;
	pa[2][1].l.punit=3;
	pa[2][2].l.pugrp=3;
	pa[2][2].l.punit=3;
	pa[2][3].l.pugrp=4;
	pa[2][3].l.punit=3;
	pa[2][4].l.pugrp=5;
	pa[2][4].l.punit=3;
	pa[2][5].l.pugrp=6;
	pa[2][5].l.punit=3;
	pa[2][6].l.pugrp=7;
	pa[2][6].l.punit=3;
	pa[2][7].l.pugrp=0;
	pa[2][7].l.punit=0;
	pa[2][8].l.pugrp=1;
	pa[2][8].l.punit=0;
	pa[2][9].l.pugrp=2;
	pa[2][9].l.punit=0;
	pa[2][10].l.pugrp=3;
	pa[2][10].l.punit=0;
	pa[2][11].l.pugrp=4;
	pa[2][11].l.punit=0;
	pa[2][12].l.pugrp=5;
	pa[2][12].l.punit=0;
	pa[2][13].l.pugrp=6;
	pa[2][13].l.punit=0;
	pa[2][14].l.pugrp=7;
	pa[2][14].l.punit=0;
	pa[2][15].l.pugrp=0;
	pa[2][15].l.punit=1;
	pa[2][16].l.pugrp=1;
	pa[2][16].l.punit=1;
	pa[2][17].l.pugrp=2;
	pa[2][17].l.punit=1;
	pa[2][18].l.pugrp=3;
	pa[2][18].l.punit=1;
	pa[2][19].l.pugrp=4;
	pa[2][19].l.punit=1;
	pa[2][20].l.pugrp=5;
	pa[2][20].l.punit=1;
	pa[2][21].l.pugrp=6;
	pa[2][21].l.punit=1;
	pa[2][22].l.pugrp=7;
	pa[2][22].l.punit=1;
	pa[2][23].l.pugrp=0;
	pa[2][23].l.punit=2;
	pa[2][24].l.pugrp=1;
	pa[2][24].l.punit=2;
	pa[2][25].l.pugrp=2;
	pa[2][25].l.punit=2;
	pa[2][26].l.pugrp=3;
	pa[2][26].l.punit=2;
	pa[2][27].l.pugrp=4;
	pa[2][27].l.punit=2;
	pa[2][28].l.pugrp=5;
	pa[2][28].l.punit=2;
	pa[2][29].l.pugrp=6;
	pa[2][29].l.punit=2;
	pa[2][30].l.pugrp=7;
	pa[2][30].l.punit=2;
	pa[3][0].l.pugrp=7;
	pa[3][0].l.punit=3;
	pa[3][1].l.pugrp=0;
	pa[3][1].l.punit=0;
	pa[3][2].l.pugrp=1;
	pa[3][2].l.punit=0;
	pa[3][3].l.pugrp=2;
	pa[3][3].l.punit=0;
	pa[3][4].l.pugrp=3;
	pa[3][4].l.punit=0;
	pa[3][5].l.pugrp=4;
	pa[3][5].l.punit=0;
	pa[3][6].l.pugrp=5;
	pa[3][6].l.punit=0;
	pa[3][7].l.pugrp=6;
	pa[3][7].l.punit=0;
	pa[3][8].l.pugrp=7;
	pa[3][8].l.punit=0;
	pa[3][9].l.pugrp=0;
	pa[3][9].l.punit=1;
	pa[3][10].l.pugrp=1;
	pa[3][10].l.punit=1;
	pa[3][11].l.pugrp=2;
	pa[3][11].l.punit=1;
	pa[3][12].l.pugrp=3;
	pa[3][12].l.punit=1;
	pa[3][13].l.pugrp=4;
	pa[3][13].l.punit=1;
	pa[3][14].l.pugrp=5;
	pa[3][14].l.punit=1;
	pa[3][15].l.pugrp=6;
	pa[3][15].l.punit=1;
	pa[3][16].l.pugrp=7;
	pa[3][16].l.punit=1;
	pa[3][17].l.pugrp=0;
	pa[3][17].l.punit=2;
	pa[3][18].l.pugrp=1;
	pa[3][18].l.punit=2;
	pa[3][19].l.pugrp=2;
	pa[3][19].l.punit=2;
	pa[3][20].l.pugrp=3;
	pa[3][20].l.punit=2;
	pa[3][21].l.pugrp=4;
	pa[3][21].l.punit=2;
	pa[3][22].l.pugrp=5;
	pa[3][22].l.punit=2;
	pa[3][23].l.pugrp=6;
	pa[3][23].l.punit=2;
	pa[3][24].l.pugrp=7;
	pa[3][24].l.punit=2;
	pa[3][25].l.pugrp=0;
	pa[3][25].l.punit=3;
	pa[3][26].l.pugrp=1;
	pa[3][26].l.punit=3;
	pa[3][27].l.pugrp=2;
	pa[3][27].l.punit=3;
	pa[3][28].l.pugrp=3;
	pa[3][28].l.punit=3;
	pa[3][29].l.pugrp=4;
	pa[3][29].l.punit=3;
	pa[3][30].l.pugrp=5;
	pa[3][30].l.punit=3;
	pa[4][0].l.pugrp=4;
	pa[4][0].l.punit=0;
	pa[4][1].l.pugrp=5;
	pa[4][1].l.punit=0;
	pa[4][2].l.pugrp=6;
	pa[4][2].l.punit=0;
	pa[4][3].l.pugrp=7;
	pa[4][3].l.punit=0;
	pa[4][4].l.pugrp=0;
	pa[4][4].l.punit=1;
	pa[4][5].l.pugrp=1;
	pa[4][5].l.punit=1;
	pa[4][6].l.pugrp=2;
	pa[4][6].l.punit=1;
	pa[4][7].l.pugrp=3;
	pa[4][7].l.punit=1;
	pa[4][8].l.pugrp=4;
	pa[4][8].l.punit=1;
	pa[4][9].l.pugrp=5;
	pa[4][9].l.punit=1;
	pa[4][10].l.pugrp=6;
	pa[4][10].l.punit=1;
	pa[4][11].l.pugrp=7;
	pa[4][11].l.punit=1;
	pa[4][12].l.pugrp=0;
	pa[4][12].l.punit=2;
	pa[4][13].l.pugrp=1;
	pa[4][13].l.punit=2;
	pa[4][14].l.pugrp=2;
	pa[4][14].l.punit=2;
	pa[4][15].l.pugrp=3;
	pa[4][15].l.punit=2;
	pa[4][16].l.pugrp=4;
	pa[4][16].l.punit=2;
	pa[4][17].l.pugrp=5;
	pa[4][17].l.punit=2;
	pa[4][18].l.pugrp=6;
	pa[4][18].l.punit=2;
	pa[4][19].l.pugrp=7;
	pa[4][19].l.punit=2;
	pa[4][20].l.pugrp=0;
	pa[4][20].l.punit=3;
	pa[4][21].l.pugrp=1;
	pa[4][21].l.punit=3;
	pa[4][22].l.pugrp=2;
	pa[4][22].l.punit=3;
	pa[4][23].l.pugrp=3;
	pa[4][23].l.punit=3;
	pa[4][24].l.pugrp=4;
	pa[4][24].l.punit=3;
	pa[4][25].l.pugrp=5;
	pa[4][25].l.punit=3;
	pa[4][26].l.pugrp=6;
	pa[4][26].l.punit=3;
	pa[4][27].l.pugrp=7;
	pa[4][27].l.punit=3;
	pa[4][28].l.pugrp=0;
	pa[4][28].l.punit=0;
	pa[4][29].l.pugrp=1;
	pa[4][29].l.punit=0;
	pa[4][30].l.pugrp=2;
	pa[4][30].l.punit=0;
	pa[5][0].l.pugrp=5;
	pa[5][0].l.punit=2;
	pa[5][1].l.pugrp=6;
	pa[5][1].l.punit=2;
	pa[5][2].l.pugrp=7;
	pa[5][2].l.punit=2;
	pa[5][3].l.pugrp=0;
	pa[5][3].l.punit=3;
	pa[5][4].l.pugrp=1;
	pa[5][4].l.punit=3;
	pa[5][5].l.pugrp=2;
	pa[5][5].l.punit=3;
	pa[5][6].l.pugrp=3;
	pa[5][6].l.punit=3;
	pa[5][7].l.pugrp=4;
	pa[5][7].l.punit=3;
	pa[5][8].l.pugrp=5;
	pa[5][8].l.punit=3;
	pa[5][9].l.pugrp=6;
	pa[5][9].l.punit=3;
	pa[5][10].l.pugrp=7;
	pa[5][10].l.punit=3;
	pa[5][11].l.pugrp=0;
	pa[5][11].l.punit=0;
	pa[5][12].l.pugrp=1;
	pa[5][12].l.punit=0;
	pa[5][13].l.pugrp=2;
	pa[5][13].l.punit=0;
	pa[5][14].l.pugrp=3;
	pa[5][14].l.punit=0;
	pa[5][15].l.pugrp=4;
	pa[5][15].l.punit=0;
	pa[5][16].l.pugrp=5;
	pa[5][16].l.punit=0;
	pa[5][17].l.pugrp=6;
	pa[5][17].l.punit=0;
	pa[5][18].l.pugrp=7;
	pa[5][18].l.punit=0;
	pa[5][19].l.pugrp=0;
	pa[5][19].l.punit=1;
	pa[5][20].l.pugrp=1;
	pa[5][20].l.punit=1;
	pa[5][21].l.pugrp=2;
	pa[5][21].l.punit=1;
	pa[5][22].l.pugrp=3;
	pa[5][22].l.punit=1;
	pa[5][23].l.pugrp=4;
	pa[5][23].l.punit=1;
	pa[5][24].l.pugrp=5;
	pa[5][24].l.punit=1;
	pa[5][25].l.pugrp=6;
	pa[5][25].l.punit=1;
	pa[5][26].l.pugrp=7;
	pa[5][26].l.punit=1;
	pa[5][27].l.pugrp=0;
	pa[5][27].l.punit=2;
	pa[5][28].l.pugrp=1;
	pa[5][28].l.punit=2;
	pa[5][29].l.pugrp=2;
	pa[5][29].l.punit=2;
	pa[5][30].l.pugrp=3;
	pa[5][30].l.punit=2;
	pa[6][0].l.pugrp=3;
	pa[6][0].l.punit=2;
	pa[6][1].l.pugrp=4;
	pa[6][1].l.punit=2;
	pa[6][2].l.pugrp=5;
	pa[6][2].l.punit=2;
	pa[6][3].l.pugrp=6;
	pa[6][3].l.punit=2;
	pa[6][4].l.pugrp=7;
	pa[6][4].l.punit=2;
	pa[6][5].l.pugrp=0;
	pa[6][5].l.punit=3;
	pa[6][6].l.pugrp=1;
	pa[6][6].l.punit=3;
	pa[6][7].l.pugrp=2;
	pa[6][7].l.punit=3;
	pa[6][8].l.pugrp=3;
	pa[6][8].l.punit=3;
	pa[6][9].l.pugrp=4;
	pa[6][9].l.punit=3;
	pa[6][10].l.pugrp=5;
	pa[6][10].l.punit=3;
	pa[6][11].l.pugrp=6;
	pa[6][11].l.punit=3;
	pa[6][12].l.pugrp=7;
	pa[6][12].l.punit=3;
	pa[6][13].l.pugrp=0;
	pa[6][13].l.punit=0;
	pa[6][14].l.pugrp=1;
	pa[6][14].l.punit=0;
	pa[6][15].l.pugrp=2;
	pa[6][15].l.punit=0;
	pa[6][16].l.pugrp=3;
	pa[6][16].l.punit=0;
	pa[6][17].l.pugrp=4;
	pa[6][17].l.punit=0;
	pa[6][18].l.pugrp=5;
	pa[6][18].l.punit=0;
	pa[6][19].l.pugrp=6;
	pa[6][19].l.punit=0;
	pa[6][20].l.pugrp=7;
	pa[6][20].l.punit=0;
	pa[6][21].l.pugrp=0;
	pa[6][21].l.punit=1;
	pa[6][22].l.pugrp=1;
	pa[6][22].l.punit=1;
	pa[6][23].l.pugrp=2;
	pa[6][23].l.punit=1;
	pa[6][24].l.pugrp=3;
	pa[6][24].l.punit=1;
	pa[6][25].l.pugrp=4;
	pa[6][25].l.punit=1;
	pa[6][26].l.pugrp=5;
	pa[6][26].l.punit=1;
	pa[6][27].l.pugrp=6;
	pa[6][27].l.punit=1;
	pa[6][28].l.pugrp=7;
	pa[6][28].l.punit=1;
	pa[6][29].l.pugrp=0;
	pa[6][29].l.punit=2;
	pa[6][30].l.pugrp=1;
	pa[6][30].l.punit=2;
	pa[7][0].l.pugrp=2;
	pa[7][0].l.punit=1;
	pa[7][1].l.pugrp=3;
	pa[7][1].l.punit=1;
	pa[7][2].l.pugrp=4;
	pa[7][2].l.punit=1;
	pa[7][3].l.pugrp=5;
	pa[7][3].l.punit=1;
	pa[7][4].l.pugrp=6;
	pa[7][4].l.punit=1;
	pa[7][5].l.pugrp=7;
	pa[7][5].l.punit=1;
	pa[7][6].l.pugrp=0;
	pa[7][6].l.punit=2;
	pa[7][7].l.pugrp=1;
	pa[7][7].l.punit=2;
	pa[7][8].l.pugrp=2;
	pa[7][8].l.punit=2;
	pa[7][9].l.pugrp=3;
	pa[7][9].l.punit=2;
	pa[7][10].l.pugrp=4;
	pa[7][10].l.punit=2;
	pa[7][11].l.pugrp=5;
	pa[7][11].l.punit=2;
	pa[7][12].l.pugrp=6;
	pa[7][12].l.punit=2;
	pa[7][13].l.pugrp=7;
	pa[7][13].l.punit=2;
	pa[7][14].l.pugrp=0;
	pa[7][14].l.punit=3;
	pa[7][15].l.pugrp=1;
	pa[7][15].l.punit=3;
	pa[7][16].l.pugrp=2;
	pa[7][16].l.punit=3;
	pa[7][17].l.pugrp=3;
	pa[7][17].l.punit=3;
	pa[7][18].l.pugrp=4;
	pa[7][18].l.punit=3;
	pa[7][19].l.pugrp=5;
	pa[7][19].l.punit=3;
	pa[7][20].l.pugrp=6;
	pa[7][20].l.punit=3;
	pa[7][21].l.pugrp=7;
	pa[7][21].l.punit=3;
	pa[7][22].l.pugrp=0;
	pa[7][22].l.punit=0;
	pa[7][23].l.pugrp=1;
	pa[7][23].l.punit=0;
	pa[7][24].l.pugrp=2;
	pa[7][24].l.punit=0;
	pa[7][25].l.pugrp=3;
	pa[7][25].l.punit=0;
	pa[7][26].l.pugrp=4;
	pa[7][26].l.punit=0;
	pa[7][27].l.pugrp=5;
	pa[7][27].l.punit=0;
	pa[7][28].l.pugrp=6;
	pa[7][28].l.punit=0;
	pa[7][29].l.pugrp=7;
	pa[7][29].l.punit=0;
	pa[7][30].l.pugrp=0;
	pa[7][30].l.punit=1;

	for (int i = 0; i < 8; i++){

		for(int j = 0; j < 31;j++){
			chunkstate[pa[i][j].l.pugrp * geo->l.npunit + pa[i][j].l.punit][pa[i][j].l.chunk].invalid_page_num = 0;
			chunkstate[pa[i][j].l.pugrp * geo->l.npunit + pa[i][j].l.punit][pa[i][j].l.chunk].sector = new bool[sector_num];
			
			for (int k = 0; k < sector_num; k++){
				chunkstate[pa[i][j].l.pugrp * geo->l.npunit + pa[i][j].l.punit][pa[i][j].l.chunk].sector[k] = 1;
			}

		}
	} 






	pa_table = pa; 


}


struct nvm_addr embedding_table_init::transfer_PA_from_init(int vec_ID, int table_ID){

	struct nvm_addr **pa = get_pa_table();
	int table_county = table_ID/write_table;
	int table_countx = table_ID%write_table;
	int sectors = table_countx*getsize_char()/byte_num;
	int pu_grp_addr = sectors/sector_num ; 
	int sector_start = sectors%sector_num;
	int sector_offset = vec_ID*get_dimention()*sizeof(float)/byte_num;
	int sec_addr = sector_start + sector_offset;
	
	if (sec_addr / sector_num > 0){
		pu_grp_addr += sec_addr / sector_num;
		sec_addr = sec_addr % sector_num;
	}
	struct nvm_addr res;
	res = pa[table_county][pu_grp_addr];
	res.l.sectr = sec_addr;

	return res;
}

// int main(){
//      embedding_table_init emb2(32,500000,64);
//      emb2.write();
//  	// struct nvm_addr **pa = emb2.get_pa_table();
//  	//emb2.pa_init();
//      //struct nvm_addr test = emb2.transfer_PA_from_init(10000, 0);	
// }

