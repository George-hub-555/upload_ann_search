terminate called after throwing an instance of 'faiss::FaissException'
  what():  Error in void faiss::IndexIVFFastScan::init_fastscan(faiss::Quantizer*, size_t, size_t, size_t, faiss::MetricType, int, bool) at /home/tysearch/g500_test/faiss/faiss-main/faiss/IndexIVFFastScan.cpp:70: Error: 'nbits_init == 4' failed
Aborted (core dumped)


tysearch 4110459 3675549 99 15:00 pts/1    00:45:51 ./bench_glove200_faisspqfs -dataset dataset/glove.twitter.27B.200d.txt -indexes all -nlist 128,256,512,1024,2048,4096 -nprobe 1,2,4,8,16,32,64,128,256 -m 100 -k_reorder 10 -k 10 -maxtrn 200000 -reportfreq 1000
tysearch 4130800 3794509  0 15:16 pts/3    00:00:00 grep --color=auto bench_glove200_faisspqfs
