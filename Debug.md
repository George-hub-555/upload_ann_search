"all_results.csv" [dos] 11L, 1256B                                                                                                                                                  1,1           All
algorithm,implementation,top_k,threads,search_parameter,search_value,recall_at_k,qps,repeat_count,warmup_queries,base_count,query_count,dimension,index_parameters,status,note
faiss-hnsw,Faiss,,,,,,,,,,,,,failed,[Errno 13] Permission denied: 'bin/ann_faiss_benchmark'
faiss-ivfpq,Faiss,,,,,,,,,,,,,failed,[Errno 13] Permission denied: 'bin/ann_faiss_benchmark'
hnswlib-hnsw,hnswlib,,,,,,,,,,,,,failed,[Errno 13] Permission denied: 'bin/ann_hnswlib_benchmark'
ngt,NGT,,,,,,,,,,,,,failed,[Errno 13] Permission denied: 'bin/ann_ngt_benchmark'
kgn,KGN,,,,,,,,,,,,,skipped,repository contains only a linux_x86_64 wheel and no ARM-buildable source
qsgngt,QSG-NGT,,,,,,,,,,,,,skipped,repository contains only x86_64 binaries/libraries and no ARM-buildable source
parlay-vamana,ParlayANN,,,,,,,,,,,,,skipped,supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled
parlay-hnsw,ParlayANN,,,,,,,,,,,,,skipped,supplied ParlayANN HNSW benchmark/search entry is incomplete; ARM build disabled
parlay-hcnng,ParlayANN,,,,,,,,,,,,,skipped,supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled
parlay-pynndescent,ParlayANN,,,,,,,,,,,,,skipped,supplied ParlayANN distance code includes x86 intrinsics; ARM build disabled
