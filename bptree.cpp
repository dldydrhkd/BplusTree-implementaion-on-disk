#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <math.h>
#include <algorithm>
#include <stack>
#define MAX 4096

using namespace std;

int num;                //block에 들어가는 entry 개수
int total_block=3;

typedef struct index_entry{         //non_leaf node의 entry
    int key;
    int index;         //a right child node's BID in the B+tree binary file
} index_entry;

typedef struct data_entry{          //leaf node의 entry
    int key;
    int value;
} data_entry;

bool cmp(const data_entry &a, const data_entry &b){             //data entry compare 함수
    if(a.key<b.key){
        return true;
    }
    return false;
}

bool cmp2(const index_entry &a, const index_entry &b){          //index entry compare 함수
    if(a.key<b.key){
        return true;
    }
    return false;
}

typedef struct header{              //index file의 header
    int block_size;
    int root_bid;
    int depth;
} header;

header read_header(char* filename){             //header 가져오기
    FILE *file=fopen(filename, "rb");
    int n[3];
    fread(n, sizeof(int), 3, file);
    header hd;
    hd.block_size=n[0];
    hd.root_bid=n[1];
    hd.depth=n[2];
    fclose(file);
    return hd;
}

void initialize_size(char* filename){               //block에 들어가는 entry 개수 계산
    FILE *file=fopen(filename,"rb");
    int temp;
    fread(&temp,sizeof(int),1,file);
    num=(temp-4)/8;
    fclose(file);
}

void insertion(int key, int value, char* filename){
    header hd=read_header(filename);
    FILE* file;
    file = fopen(filename,"r+b");
    fflush(file);
    fseek(file, 12+(hd.root_bid-1)*hd.block_size, SEEK_SET);        //첫 노드

    if(hd.depth==1){                //깊이가 1일 경우
        bool created=true;          //첫 노드가 생성 되었는지 판별
        vector<data_entry> data;
        for(int i=0; i<num; i++){
            int temp_key=fgetc(file);           //데이터를 하나 읽어온다
            if(temp_key==-1){                   //첫 노드가 없는 경우
                int n[MAX]={0,};
                fwrite(&key,sizeof(int),1,file);        //key를 삽입
                fwrite(&value,sizeof(int),1,file);      //value 삽입
                fwrite(n,sizeof(int),(hd.block_size-8)/4,file);     //나머지 0으로 채움
                created=false;                         
                break;
            }
            int temp_value;
            fseek(file,-1,SEEK_CUR);
            fread(&temp_key,sizeof(int),1,file);                //key 읽어오기
            fread(&temp_value,sizeof(int),1,file);              //value 읽어오기
            if(temp_key==0){
                break;
            }

            data.push_back({temp_key,temp_value});              //data vector에 저장

        }
        data.push_back({key,value});                        //data에 새로운 key, value 저장
        
        if(created){                                    //첫 노드가 있었다면
            sort(data.begin(), data.end(),cmp);             //data vector 정렬
            
            if(data.size()>num){                        //full이면
                vector<data_entry> child1;              //child1 leaf
                vector<data_entry> child2;              //child2 leaf
                fseek(file,12+(hd.root_bid-1)*hd.block_size,SEEK_SET);          //첫 번째 노드로 위치

                //data를 split
                for(int i=0; i<data.size(); i++){                   
                    if(i<data.size()/2){
                        child1.push_back(data[i]);
                    }
                    else{
                        child2.push_back(data[i]);
                    }
                }

                //child1 데이터 저장
                for(int i=0; i<num; i++){
                    if(i<child1.size()){
                        fwrite(&child1[i],sizeof(int),2, file);
                    }
                    else{
                        int n[MAX];
                        fwrite(n,sizeof(int),2,file);
                    }
                }

                int next=2;         //다음 노드
                fwrite(&next,sizeof(int),1,file);

                //child2 데이터 저장
                for(int i=0; i<num; i++){
                    if(i<child2.size()){
                        fwrite(&child2[i],sizeof(int),2,file);
                    }
                    else{
                        int n[MAX];
                        fwrite(n,sizeof(int),2,file);
                    }
                }
                next=0;
                fwrite(&next,sizeof(int),1,file);       //마지막 leaf node는 아무것도 가르키지 않음

                next=1;                                 //새로 생긴 root node가 1을 가르킴
                fwrite(&next,sizeof(int),1,file);
                int s=ceil(data.size()/2);
                fwrite(&data[s].key,sizeof(int),1,file);        //가운데 값을 가져옴
                next=2;
                fwrite(&next,sizeof(int),1,file);           //2번쨰 노드를 가르킴
                int n[MAX];
                fwrite(n,sizeof(int),(hd.block_size-12)/4,file);     //나머지 0으로 set
                hd.root_bid=3;
                hd.depth+=1;
                fseek(file,4,SEEK_SET);
                fwrite(&hd.root_bid,sizeof(int),1,file);
                fwrite(&hd.depth,sizeof(int),1,file);
            }
            
            else{                                       //full이 아니면 그냥 집어 넣는다
                fseek(file, 12+(hd.root_bid-1)*hd.block_size, SEEK_SET);
                for(int i=0; i<data.size(); i++){
                    fwrite(&data[i],sizeof(int),2,file);
                }
            }
        }
    }

    else{
        stack<int> path;                            //path 저장
        path.push(hd.root_bid);
        int bid=hd.root_bid;                        //첫 node bid
        for(int i=1; i<=hd.depth; i++){             //depth 1부터 시작
            fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);       //첫 루트노드
            
            //searching
            if(i!=hd.depth){                //non_leaf인 경우
                int less_next;              
                fread(&less_next,sizeof(int),1,file);       //왼쪽 child 저장
                for(int j=0; j<num; j++){
                    int key_temp;                       //key값
                    fread(&key_temp,sizeof(int),1,file);
                    if(key_temp==0){                //key가 0이면 마지막 으로 감
                        break;
                    }
                    int more_next;                  //오른쪽
                    fread(&more_next,sizeof(int),1,file);  
                    if(key<key_temp){               //key가 key_temp보다 작으면 
                        break;
                    }
                    less_next=more_next;
                }
                path.push(less_next);               //path 저장
                bid=less_next;                  //bid를 다음 노드로 바꾼다
            }

            //found
            else{                           //leaf node인 경우
                vector<data_entry> temp;

                for(int i=0; i<num; i++){                   //leaf node 정보를 가져옴
                    int key_temp;                           //key값
                    fread(&key_temp,sizeof(int),1,file);
                    int value_temp;                         //value값
                    fread(&value_temp,sizeof(int),1,file);
                    if(key_temp==0){                        //0이면 더 이상 없는거
                        break;
                    }
                    temp.push_back({key_temp,value_temp});
                }
                temp.push_back({key,value});
                sort(temp.begin(),temp.end(),cmp);          //leaf node에 insert 후 정렬

                if(temp.size()>num){                        //leaf node가 꽉찬 경우
                    int child1_id=bid;                      //현재 bid 저장
                    int child2_id=total_block+1;            //새로운 노드 블럭 아이디
                    total_block+=1;

                    int child1_next=child2_id;              //child1은 새로운 노드를 가르킨다
                    int child2_next;                        //child2_next는 child1이 가르키던걸 가르킴
                    fseek(file, 12+(child1_id-1)*hd.block_size+num*8, SEEK_SET);
                    fread(&child2_next,sizeof(int),1,file);     //child2가 가르킬 id를 가져옴

                    vector<data_entry> child1;          //child1 data
                    vector<data_entry> child2;          //child2 data

                    //split을 한다
                    for(int i=0; i<temp.size(); i++){
                        if(i<temp.size()/2){
                            child1.push_back(temp[i]);   
                        }
                        else{
                            child2.push_back(temp[i]);
                        }
                    }

                    fseek(file, 12+(child1_id-1)*hd.block_size, SEEK_SET);  //child1으로 감

                    //left leaf node에 저장
                    for(int i=0; i<num; i++){
                        if(i<child1.size()){
                            fwrite(&child1[i],sizeof(int),2, file);
                        }
                        else{
                            int n[MAX]={0,};
                            fwrite(n,sizeof(int),2,file);
                        }
                    }
                    fwrite(&child1_next,sizeof(int),1,file);        //다음 노드

                    //right leaf_node에 저장
                    fseek(file, 12+(total_block-1)*hd.block_size, SEEK_SET);
                    for(int i=0; i<num; i++){
                        if(i<child2.size()){
                            fwrite(&child2[i],sizeof(int),2, file);
                        }
                        else{
                            int n[MAX]={0,};
                            fwrite(n,sizeof(int),2,file);
                        }
                    }
                    fwrite(&child2_next,sizeof(int),1,file);        //다음 노드
                    
                    path.pop();  //현재 노드 꺼냄

                    //부모에 key insert
                    if(!path.empty()){
                        int parent_id=path.top();           //leaf의 부모
                        int s=ceil(temp.size()/2);          //중간 찾기
                        int new_key=temp[s].key;            //새로운 key
                        int new_index=total_block;          //새로운 index
                        path.pop();                         //부모 노드 꺼냄

                        vector<index_entry> parent;             //leaf의 부모

                        parent.push_back({new_key,new_index});   //parent에 새로운 entry추가

                        //부모 정보 가져오기
                        fseek(file, 12+(parent_id-1)*hd.block_size, SEEK_SET);          //부모로 위치
                        int left_child_id;                                  
                        fread(&left_child_id,sizeof(int),1,file);           //left child id 가져오기
                        for(int i=0; i<num; i++){                           //0이 나올때까지 읽고 vector에 추가
                            int key_temp2;
                            fread(&key_temp2, sizeof(int), 1, file);
                            if(key_temp2==0){
                                break;
                            }
                            int index_temp2;
                            fread(&index_temp2, sizeof(int), 1, file);
                            parent.push_back({key_temp2, index_temp2});
                        }
                        
                        sort(parent.begin(), parent.end(), cmp2);       //key insert후 정렬

                        //부모가 꽉찬 경우
                        int parent_size=parent.size();
                        if(parent_size>num){         //부모 노드가 꽉찬 경우 부모 split

                            if(!path.empty()){                      //leaf node의 부모가 root가 아닌 경우
                                int now_parent_id=parent_id;            //현재 꽉찬 parent 저장
                                int grand_id=path.top();                //할아버지 저장
                                int new_parent_id=total_block+1;          //새로 만들 parent저장
                                int left_idx=left_child_id;
                                parent_size=parent.size();
                                total_block+=1;
                                //부모 split;
                                while(!path.empty()){           
                                    path.pop();
                                    if(!path.empty()){              //부모가 root인 경우 grand가 root인 경우
                                        vector<index_entry> parent1;
                                        vector<index_entry> parent2;
                                        vector<index_entry> grand;
                                        int new_grand_key;
                                        int new_grand_index;

                                        //split
                                        for(int i=0; i<parent.size(); i++){         
                                            if(i==parent_size/2){
                                                new_grand_key=parent[i].key;
                                                new_grand_index=parent[i].index;
                                            }
                                            else if(i<parent_size/2){
                                                parent1.push_back(parent[i]);
                                            }
                                            else if(i>parent_size/2){
                                                parent2.push_back(parent[i]);
                                            }
                                        }

                                        //현재 split 해야할 부모노드로 이동
                                        fseek(file, 12+(now_parent_id-1)*hd.block_size,SEEK_SET);
                                        fwrite(&left_idx, sizeof(int), 1, file);
                                        for(int i=0; i<num; i++){                       //부모 노드 block에 저장
                                            if(i<parent1.size()){
                                                fwrite(&parent1[i],sizeof(int),2,file);
                                            }
                                            else{
                                                int n[MAX]={0,};
                                                fwrite(n,sizeof(int),2,file);
                                            }
                                        }

                                        //새로 생긴 부모노드로 이동
                                        fseek(file, 12+(new_parent_id-1)*hd.block_size, SEEK_SET);
                                        fwrite(&new_grand_index, sizeof(int), 1, file);
                                        for(int i=0; i<num; i++){
                                            if(i<parent2.size()){
                                                fwrite(&parent2[i],sizeof(int),2,file);
                                            }
                                            else{
                                                int n[MAX]={0,};
                                                fwrite(n,sizeof(int),2,file);
                                            }
                                        }

                                        //grand가 root인 노드로 이동
                                        //정보 가져오기
                                        fseek(file, 12+(grand_id-1)*hd.block_size, SEEK_SET);
                                        int left;
                                        fread(&left, sizeof(int),1, file);
                                        for(int i=0; i<num; i++){
                                            int temp_grand_key;
                                            fread(&temp_grand_key, sizeof(int),1,file);
                                            if(temp_grand_key==0){
                                                break;
                                            }
                                            int temp_grand_idx;
                                            fread(&temp_grand_idx, sizeof(int), 1, file);
                                            grand.push_back({temp_grand_key, temp_grand_idx});
                                        }
                                        grand.push_back({new_grand_key,total_block});
                                        sort(grand.begin(),grand.end(),cmp2);

                                        
                                        if(grand.size()>num){           //root가 꽉 찰 경우
                                            vector<index_entry> temp1;
                                            vector<index_entry> temp2;
                                            vector<index_entry> root;

                                            int temp_new_key;
                                            int temp_new_index;
                                            int temp_left_new_index=grand_id;
                                            int temp_right_new_index=total_block+1;
                                            total_block+=2;

                                            for(int i=0; i<grand.size(); i++){
                                                if(i==grand.size()/2){
                                                    temp_new_key=grand[i].key;
                                                    temp_new_index=grand[i].index;
                                                }
                                                else if(i<grand.size()/2){
                                                    temp1.push_back(grand[i]);
                                                }
                                                else if(i>grand.size()/2){
                                                    temp2.push_back(grand[i]);
                                                }
                                            }

                                            fseek(file,12+(grand_id-1)*hd.block_size,SEEK_SET);
                                            fwrite(&left,sizeof(int),1,file);
                                            for(int i=0; i<num; i++){
                                                if(i<temp1.size()){
                                                    fwrite(&temp1[i],sizeof(int),2,file);
                                                }
                                                else{
                                                    int n[MAX]={0,};
                                                    fwrite(n,sizeof(int),2,file);
                                                }
                                            }

                                            fseek(file,12+(temp_right_new_index-1)*hd.block_size,SEEK_SET);
                                            fwrite(&temp_new_index,sizeof(int),1,file);
                                            for(int i=0; i<num; i++){
                                                if(i<temp2.size()){
                                                    fwrite(&temp2[i],sizeof(int),2,file);
                                                }
                                                else{
                                                    int n[MAX]={0,};
                                                    fwrite(n,sizeof(int),2,file);
                                                }
                                            }

                                            fseek(file,12+(total_block-1)*hd.block_size,SEEK_SET);
                                            fwrite(&temp_left_new_index,sizeof(int),1,file);
                                            fwrite(&temp_new_key,sizeof(int),1,file);
                                            fwrite(&temp_right_new_index,sizeof(int),1,file);
                                            int n[MAX]={0,};
                                            fwrite(n,sizeof(int),(hd.block_size-12)/4,file);

                                            rewind(file);
                                            fseek(file,4,SEEK_SET);
                                            fwrite(&total_block,sizeof(int),1,file);
                                            int dp=hd.depth+1;
                                            fwrite(&dp,sizeof(int),1,file);
                                            break;
                                        }
                                        //root가 다 안 찬 경우 그냥 저장
                                        else{
                                            fseek(file, 12+(grand_id-1)*hd.block_size, SEEK_SET);
                                            fwrite(&left, sizeof(int),1,file);
                                            for(int i=0; i<num; i++){
                                                if(i<grand.size()){
                                                    fwrite(&grand[i],sizeof(int),2,file);
                                                }
                                                else{
                                                    int n[MAX];
                                                    fwrite(n,sizeof(int),2,file);
                                                }
                                            }
                                            break;
                                        }
                                    }
                                    //grand가 root가 아닌 경우
                                    else{
                                        vector<index_entry> parent1;            //첫번째 parent저장
                                        vector<index_entry> parent2;            //두번째 parent저장
                                        vector<index_entry> grand;              //할아버지 저장
                                        int new_grand_key;                      //할아버지에 들어갈 새 키
                                        int new_grand_index;                    //할아버지에 들어갈 새 키
                                        
                                        //split
                                        for(int i=0; i<parent.size(); i++){
                                            if(i==parent_size/2){
                                                new_grand_key=parent[i].key;
                                                new_grand_index=parent[i].index;
                                            }
                                            else if(i<parent_size/2){
                                                parent1.push_back(parent[i]);
                                            }
                                            else if(i>parent_size/2){
                                                parent2.push_back(parent[i]);
                                            }
                                        }

                                        //split해야하는 부모노드 저장
                                        fseek(file, 12+(now_parent_id-1)*hd.block_size,SEEK_SET);
                                        fwrite(&left_idx, sizeof(int),1,file);
                                        for(int i=0; i<num; i++){
                                            if(i<parent1.size()){
                                                fwrite(&parent1[i],sizeof(int),2,file);
                                            }
                                            else{
                                                int n[MAX]={0,};
                                                fwrite(n,sizeof(int),2,file);
                                            }
                                        }

                                        //새로 생긴 부모노드 저장
                                        fseek(file, 12+(new_parent_id-1)*hd.block_size, SEEK_SET);
                                        fwrite(&new_grand_index, sizeof(int), 1, file);
                                        for(int i=0; i<num; i++){
                                            if(i<parent2.size()){
                                                fwrite(&parent2[i],sizeof(int),2,file);
                                            }
                                            else{
                                                int n[MAX]={0,};
                                                fwrite(n,sizeof(int),2,file);
                                            }
                                        }

                                        //할아버지 정보 가져오기
                                        fseek(file, 12+(grand_id-1)*hd.block_size, SEEK_SET);
                                        int left;
                                        fread(&left, sizeof(int),1, file);
                                        for(int i=0; i<num; i++){
                                            int temp_grand_key;
                                            fread(&temp_grand_key, sizeof(int),1,file);
                                            if(temp_grand_key==0){
                                                break;
                                            }
                                            int temp_grand_idx;
                                            fread(&temp_grand_idx, sizeof(int), 1, file);
                                            grand.push_back({temp_grand_key, temp_grand_idx});
                                        }
                                        grand.push_back({new_grand_key,total_block});
                                        sort(grand.begin(),grand.end(),cmp2);


                                        //할아버지가 꽉찬 경우
                                        if(grand.size()>num){
                                            now_parent_id=grand_id;                     //부모를 할아버지로 바꿈
                                            new_parent_id=total_block+1;                //block size 증가 할아버지를 나눌꺼라서
                                            parent.clear();                             //parent clear
                                            parent_size=grand.size();                   //부모 사이즈 할아버지로 변경
                                            total_block+=1;                             
                                            left_idx=left;                              //할아버지 왼쪽에 있는거 저장
                                            for(int i=0; i<grand.size(); i++){          //할아버지꺼 parent로 저장
                                                parent[i]=grand[i];
                                            }
                                        }

                                        //할아버지가 안 찬 경우
                                        else{
                                            fseek(file, 12+(grand_id-1)*hd.block_size, SEEK_SET);
                                            fwrite(&left, sizeof(int),1,file);
                                            for(int i=0; i<num; i++){
                                                if(i<grand.size()){
                                                    fwrite(&grand[i],sizeof(int),2,file);
                                                }
                                                else{
                                                    int n[MAX];
                                                    fwrite(n,sizeof(int),2,file);
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                            else{                   //leaf node의 부모가 root인 경우
                                vector<index_entry> parent1;
                                vector<index_entry> parent2;
                                vector<index_entry> root1;
                                
                                int parent_new_key;
                                int parent_new_index;
                                int parent_left_new_index=parent_id;
                                int parent_right_new_index=total_block+1;
                                total_block+=2;             //root node+non_leaf 추가

                                for(int i=0; i<parent.size(); i++){
                                    if(i==parent_size/2){
                                        parent_new_key=parent[i].key;
                                        parent_new_index=parent[i].index;
                                    }
                                    else if(i<parent_size/2){
                                        parent1.push_back(parent[i]);
                                    }
                                    else if(i>parent_size/2){
                                        parent2.push_back(parent[i]);
                                    }
                                }

                                fseek(file,12+(parent_id-1)*hd.block_size, SEEK_SET);    //부모1 입력
                                fwrite(&left_child_id,sizeof(int),1, file);
                                for(int i=0; i<num; i++){
                                    if(i<parent1.size()){
                                        fwrite(&parent1[i],sizeof(int),2,file);
                                    }
                                    else{
                                        int n[MAX]={0,};
                                        fwrite(n,sizeof(int),2,file);
                                    }
                                }

                                fseek(file,12+(parent_right_new_index-1)*hd.block_size, SEEK_SET);
                                fwrite(&parent_new_index,sizeof(int),1,file);
                                for(int i=0; i<num; i++){
                                    if(i<parent2.size()){
                                        fwrite(&parent2[i],sizeof(int),2,file);
                                    }
                                    else{
                                        int n[MAX]={0,};
                                        fwrite(n,sizeof(int),2,file);
                                    }
                                }


                                fseek(file,12+(total_block-1)*hd.block_size, SEEK_SET);
                                fwrite(&parent_left_new_index,sizeof(int),1,file);
                                fwrite(&parent_new_key,sizeof(int),1,file);
                                fwrite(&parent_right_new_index,sizeof(int),1,file);
                                int n[MAX]={0,};
                                fwrite(n,sizeof(int),(hd.block_size-12)/4,file);


                                rewind(file);
                                fseek(file,4,SEEK_SET);
                                fwrite(&total_block,sizeof(int),1,file);
                                int dp=hd.depth+1;
                                fwrite(&dp,sizeof(int),1,file);

                            }

                        }
                        //안 찬 경우
                        else{
                            fseek(file, 12+(parent_id-1)*hd.block_size, SEEK_SET);
                            fwrite(&left_child_id,sizeof(int),1,file);
                            for(int i=0; i<parent.size(); i++){
                                fwrite(&parent[i],sizeof(int),2,file);
                            }
                            break;
                        }
                    }
                }
                else{                                               //leaf node가 비어있는 경우
                    fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);
                    for(int i=0; i<temp.size(); i++){
                        fwrite(&temp[i],sizeof(int),2,file);
                    }
                }
            }
        }
    }
    fclose(file);
}

void create(char* filename, char* size){            //create file and initialize root node
    FILE *fp;
    int BlockSize=atoi(size);
    header hd={BlockSize,1,1};
    fp = fopen(filename,"wb");
    fwrite(&hd,sizeof(int),3,fp);
    fclose(fp);
    return;
}

void insert(char* filename, char* inputfile){
    initialize_size(filename);
    FILE *input;
    input = fopen(inputfile, "rb");
    if(input!=NULL){
        char strTemp[10];
        char *pStr;
        while(!feof(input)){
            pStr=fgets(strTemp, sizeof(strTemp), input);
            if(pStr!=NULL){
                int toknum=0;
                const char delimiters[]=",";             //,를 기준으로 잘라 읽기
                char *token =strtok(pStr, delimiters);
                int key, val;
                while(token != NULL){
                    toknum++;
                    if(toknum==1){
                        key=atoi(token);        //key값 저장
                    }
                    if(toknum==2){
                        val=atoi(token);        //val값 저장
                    }
                    token =strtok(NULL, delimiters);
                }
                insertion(key,val,filename);
            }
        }
        fclose(input);
    }
    else{
        printf("데이터가 없습니다.");
    }
    return;
}
void search_value(int key, char* filename, char *output){
    FILE* file;
    file=fopen(filename, "rb");
    
    header hd=read_header(filename);
    int num1=(hd.block_size-4)/8;
    stack<int> path;
    int bid=hd.root_bid;
    for(int i=1; i<=hd.depth; i++){             //depth 1부터 시작
            fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);       //첫 루트노드
            
            //searching
            if(i!=hd.depth){                //non_leaf인 경우
                int less_next;              
                fread(&less_next,sizeof(int),1,file);       //왼쪽 child 저장
                for(int j=0; j<num1; j++){
                    int key_temp;                       //key값
                    fread(&key_temp,sizeof(int),1,file);
                    if(key_temp==0){                //key가 0이면 마지막 으로 감
                        break;
                    }
                    int more_next;                  //오른쪽
                    fread(&more_next,sizeof(int),1,file);  
                    if(key<key_temp){               //key가 key_temp보다 작으면 
                        break;
                    }
                    less_next=more_next;
                }
                path.push(less_next);               //path 저장
                bid=less_next;                  //bid를 다음 노드로 바꾼다
            }
            else{
                FILE *fi;
                fi=fopen(output,"a");
                for(int i=0; i<num1; i++){
                    int key_temp;
                    fread(&key_temp,sizeof(int),1,file);
                    int value_temp;
                    fread(&value_temp,sizeof(int),1,file);

                    if(key_temp==key){
                        fprintf(fi,"%d,%d\n",key_temp,value_temp);
                    }
                }
                fclose(fi);
            }
    }
}

void search(char* filename, char* inputfile, char* output){
    FILE *file;
    file = fopen(inputfile, "rt");
    char buf[10];
    if(file!=NULL){
        while(!feof(file)){
            fgets(buf,100,file);
            int a=atoi(buf);
            search_value(a, filename, output);

        }
    }
    fclose(file);
    return;
}

void print(){
    printf("p");
    return;
}

void range_search(int key1, int key2, char* filename, char* output){
    FILE* file;
    file=fopen(filename, "rb");
    
    header hd=read_header(filename);
    int num1=(hd.block_size-4)/8;
    stack<int> path;
    int bid=hd.root_bid;
    vector<data_entry> v;
    for(int i=1; i<=hd.depth; i++){             //depth 1부터 시작
        fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);       //첫 루트노드
            
            //searching
        if(i!=hd.depth){                //non_leaf인 경우
            int less_next;              
            fread(&less_next,sizeof(int),1,file);       //왼쪽 child 저장
            for(int j=0; j<num1; j++){
                int key_temp;                       //key값
                fread(&key_temp,sizeof(int),1,file);
                if(key_temp==0){                //key가 0이면 마지막 으로 감
                    break;
                }
                int more_next;                  //오른쪽
                fread(&more_next,sizeof(int),1,file);  
                if(key1<key_temp){               //key가 key_temp보다 작으면 
                    break;
                }
                less_next=more_next;
            }
            path.push(less_next);               //path 저장
            bid=less_next;                  //bid를 다음 노드로 바꾼다
        }
        else{
            break;
        }
    }
    fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);
    for(int i=0; i<num1; i++){
        int key_temp;
        fread(&key_temp,sizeof(int),1,file);
        int value_temp;
        fread(&value_temp,sizeof(int),1,file);
        if(key_temp<=key2&&key_temp>=key1){
            v.push_back({key_temp,value_temp});
        }
    }
    int next;
    fread(&next,sizeof(int),1,file);
    while(next!=0){
        fseek(file, 12+(next-1)*hd.block_size, SEEK_SET);
        for(int i=0; i<num1; i++){
            int key_temp;
            fread(&key_temp,sizeof(int),1,file);
            int value_temp;
            fread(&value_temp,sizeof(int),1,file);
            if(key_temp<=key2&&key_temp>=key1){
                v.push_back({key_temp,value_temp});
            }
        }
        fread(&next,sizeof(int),1,file);
    }
    FILE *fi;
    fi=fopen(output,"a");
    for(int i=0; i<v.size(); i++){
        fprintf(fi,"%d,%d ",v[i].key,v[i].value);
    }
    fprintf(fi,"\n");
    fclose(fi);
    return;
}


void range(char* filename, char* inputfile, char* output){
    FILE *input;
    input = fopen(inputfile, "rb");
    if(input!=NULL){
        char strTemp[10];
        char *pStr;
        while(!feof(input)){
            pStr=fgets(strTemp, sizeof(strTemp), input);
            if(pStr!=NULL){
                int toknum=0;
                const char delimiters[]=",";             //,를 기준으로 잘라 읽기
                char *token =strtok(pStr, delimiters);
                int key1, key2;
                while(token != NULL){
                    toknum++;
                    if(toknum==1){
                        key1=atoi(token);        //key값 저장
                    }
                    if(toknum==2){
                        key2=atoi(token);        //val값 저장
                    }
                    token =strtok(NULL, delimiters);
                }
                range_search(key1,key2,filename,output);
            }
        }
        fclose(input);
    }
    else{
        printf("데이터가 없습니다.");
    }
    return;
}

void print(char* filename, char* output){
    FILE *file;
    file = fopen(filename, "rb");
    header hd=read_header(filename);
    vector<int> v1;
    vector<int> v2;
    int num1=(hd.block_size-4)/8;
    int bid=hd.root_bid;
    fseek(file, 12+(bid-1)*hd.block_size, SEEK_SET);
    vector<int> child;
    int child_id;
    fread(&child_id,sizeof(int),1,file);
    child.push_back(child_id);
    for(int i=0; i<num1; i++){
        int key_temp;
        int index_temp;
        fread(&key_temp,sizeof(int),1,file);
        if(key_temp==0){
            break;
        }
        fread(&index_temp,sizeof(int),1,file);
        v1.push_back(key_temp);
        child.push_back(index_temp);
    }
    for(int i=0; i<child.size(); i++){
        fseek(file, 12+(child[i]-1)*hd.block_size, SEEK_SET);
        int left_child_idx;
        fread(&left_child_idx,sizeof(int),1,file);
        for(int j=0; j<num1; j++){
            int key_temp;
            int index_temp;
            fread(&key_temp,sizeof(int),1,file);
            if(key_temp==0){
                break;
            }
            fread(&index_temp,sizeof(int),1,file);
            v2.push_back(key_temp);
        }
    }

    FILE *fi;
    fi=fopen(output,"a");
    fprintf(fi,"<0>\n");
    for(int i=0; i<v1.size(); i++){
        fprintf(fi,"%d,",v1[i]);
    }
    fprintf(fi,"\n");
    fprintf(fi,"<1>\n");
    for(int i=0; i<v2.size(); i++){
        fprintf(fi,"%d,",v2[i]);
    }
    fprintf(fi,"\n");
    fclose(fi);

}

int main(int argc, char *argv[]){
    char command=argv[1][0];
    switch(command){
        case 'c' :
            create(argv[2], argv[3]);   // create index file
            break;
        case 'i' :
            insert(argv[2], argv[3]);// insert records from [records data file], ex) records.txt
            break;
        case 's' :
            search(argv[2], argv[3], argv[4]);// search keys in [input file] and print results to [output file]
            break;
        case 'r' :
            range(argv[2], argv[3], argv[4]);// search range keys in [input file] and print results to [output file]
            break; 
        case 'p' :
            print(argv[2], argv[3]);// print B+-Tree structure to [output file]
            break;
    }
    return 0;
}














