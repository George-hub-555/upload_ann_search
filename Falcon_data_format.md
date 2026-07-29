tar -tzvf /实际路径/data.tar.gz | head -100

tar -xOf /实际路径/data.tar.gz "压缩包内/base文件路径" \
  | head -c 64 | xxd -g 1


1kw_64dim_test_data/
1kw_64dim_test_data/index_model_relevance_softLtrAfmConditionv1
1kw_64dim_test_data/conditionv1_3500_one_tier_log_form.txt
1kw_64dim_test_data/falcon_data.tar.gz
1kw_64dim_test_data/falcon_request_new.txt
1kw_64dim_test_data/falcon_request.txt

/opt/huawei/data2/l00856060/1kw_64dim_test_data.zip
