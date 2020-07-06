# BplusTree-implementaion-on-disk


* ## b+tree 구현
  -disk에 b+tree구조로 data를 저장할 수 있게 구현

* ## 기능
  1. Index creation
    bptree.exe c [bptree binary file] [block_size]
    
        -bptree header 설정
    
    
    
  2. Insertion
    bptree.exe i [bptree binary file] [records data text file]
    
        -data insertion

  3. Point (exact) search
    bptree.exe s [bptree binary file] [input text file] [output text file]
    
        -찾고자하는 key의 value를 print
    
    

  4. Range search
    bptree.exe r [bptree binary file] [input text file] [output text file]
    
        -구간 a부터 b까지의 key에 대한 key를 print
    

  5. Print B+ tree
    bptree.exe p [bptree binary file] [output text file]
    
        -root와 root의 child 노드를 print
    
