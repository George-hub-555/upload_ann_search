chmod +x devel/builder/bazel-7.4.1-linux-arm64

cd /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data

find . -maxdepth 2 -type f -printf '%P\n' | sort

**#Get Format**
ls -lh falcon_request.txt falcon_request_new.txt

file falcon_request.txt falcon_request_new.txt

wc -lc falcon_request.txt falcon_request_new.txt


head -n 3 falcon_request.txt | cut -c1-1500

head -n 3 falcon_request_new.txt | cut -c1-1500


grep -aEn -m 10 \
  'corpus|main_tier|json_query|vector|result_num|retrieve_num|request_id' \
  falcon_request.txt

grep -aEn -m 10 \
  'corpus|main_tier|json_query|vector|result_num|retrieve_num|request_id' \
  falcon_request_new.txt


awk -F',' 'NR <= 3 {print "line=" NR, "comma_fields=" NF}' \
  falcon_request.txt

awk -F',' 'NR <= 3 {print "line=" NR, "comma_fields=" NF}' \
  falcon_request_new.txt


awk 'NR <= 3 {print "line=" NR, "space_fields=" NF}' \
  falcon_request.txt

awk 'NR <= 3 {print "line=" NR, "space_fields=" NF}' \
  falcon_request_new.txt



xxd -l 128 falcon_request.txt

xxd -l 128 falcon_request_new.txt
**#Error**

