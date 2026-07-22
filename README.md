# upload_ann_search
upload ann search for arm


instruct :
cmake -B build . -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DFAISS_OPT_LEVEL=generic

make -C build -j$(nproc)
