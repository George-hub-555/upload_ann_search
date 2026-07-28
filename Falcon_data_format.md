tar -xf "1kw_64dim_test_data.zip" -O "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tf - | Select-Object -First 50
tar -xf "1kw_64dim_test_data.zip" -O "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tzf - | head -50

bash: Select-Object: command not found
tar: This does not look like a tar archive
tar: Skipping to next header
tar: Archive contains ‘\031\260\316g\376\275\327z\233\002\376i’ where numeric off_t value expected
tar: Archive contains ‘1Z\006G\255\360\233<h\255\036v’ where numeric off_t value expected
tar: Archive contains ‘\246'$`\243\237\277Q<GZ\021’ where numeric off_t value expected
tar: Archive contains ‘\357\362\300?\2379\367b\366\327Qj’ where numeric off_t value expected
tar: Archive contains ‘\344\305.\235\272\020\370o\365\346O2’ where numeric off_t value expected

tar -xf "1kw_64dim_test_data.zip" -O "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tf - | Select-Object -First 50
bash: Select-Object: command not found
tar: This does not look like a tar archive
tar: Skipping to next header
tar: Archive contains ‘\031\260\316g\376\275\327z\233\002\376i’ where numeric off_t value expected
tar: Archive contains ‘1Z\006G\255\360\233<h\255\036v’ where numeric off_t value expected
tar: Archive contains ‘\246'$`\243\237\277Q<GZ\021’ where numeric off_t value expected
tar: Archive contains ‘\357\362\300?\2379\367b\366\327Qj’ where numeric off_t value expected
tar: Archive contains ‘\344\305.\235\272\020\370o\365\346O2’ where numeric off_t value expected
^C
(base) [tysearch@wh-tysearch-ecs-bridge l00856060]$ tar -xf "1kw_64dim_test_data.zip" -O "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tzf - | head -50
tar: This does not look like a tar archive
tar: Skipping to next header
tar: Archive contains ‘\031\260\316g\376\275\327z\233\002\376i’ where numeric off_t value expected
tar: Archive contains ‘1Z\006G\255\360\233<h\255\036v’ where numeric off_t value expected
tar: Archive contains ‘\246'$`\243\237\277Q<GZ\021’ where numeric off_t value expected
tar: Archive contains ‘\357\362\300?\2379\367b\366\327Qj’ where numeric off_t value expected
tar: Archive contains ‘\344\305.\235\272\020\370o\365\346O2’ where numeric off_t value expected
tar: Archive contains ‘\246\357t\267\230*\\[\036hI\347’ where numeric off_t value expected
tar: 1kw_64dim_test_data/falcon_data.tar.gz: Not found in archive
tar: Exiting with failure status due to previous errors

gzip: stdin: unexpected end of file
tar: Child returned status 1
tar: Error is not recoverable: exiting now

unzip -l "1kw_64dim_test_data.zip"
unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tzf - | head -50


alcon_data/
falcon_data/0/
falcon_data/0/range_field/
falcon_data/0/range_field/range.range_field.204
falcon_data/0/range_field/range.range_field.565
falcon_data/0/range_field/range.range_field.523
falcon_data/0/range_field/range.range_field.717
falcon_data/0/range_field/range.range_field.686
falcon_data/0/range_field/range.range_field.820
falcon_data/0/range_field/range.range_field.17
falcon_data/0/range_field/range.range_field.156
falcon_data/0/range_field/range.range_field.450
falcon_data/0/range_field/range.range_field.567
falcon_data/0/range_field/range.range_field.952
falcon_data/0/range_field/range.range_field.329
falcon_data/0/range_field/range.range_field.65
falcon_data/0/range_field/range.range_field.101
falcon_data/0/range_field/range.range_field.851
falcon_data/0/range_field/range.range_field.415
falcon_data/0/range_field/range.range_field.316
falcon_data/0/range_field/range.range_field.732
falcon_data/0/range_field/range.range_field.319
falcon_data/0/range_field/range.range_field.107
falcon_data/0/range_field/range.range_field.124
falcon_data/0/range_field/range.range_field.349
falcon_data/0/range_field/range.range_field.867
falcon_data/0/range_field/range.range_field.189
falcon_data/0/range_field/range.range_field.264
falcon_data/0/range_field/range.range_field.248
falcon_data/0/range_field/range.range_field.591
falcon_data/0/range_field/range.range_field.594
falcon_data/0/range_field/range.range_field.615
falcon_data/0/range_field/range.range_field.861
falcon_data/0/range_field/range.range_field.501
falcon_data/0/range_field/range.range_field.656
falcon_data/0/range_field/range.range_field.202
falcon_data/0/range_field/range.range_field.344
falcon_data/0/range_field/range.range_field.988
falcon_data/0/range_field/range.range_field.723
falcon_data/0/range_field/range.range_field.51
falcon_data/0/range_field/range.range_field.961
falcon_data/0/range_field/range.range_field.903
falcon_data/0/range_field/range.range_field.82
falcon_data/0/range_field/range.range_field.452
falcon_data/0/range_field/range.range_field.886
falcon_data/0/range_field/range.range_field.477
falcon_data/0/range_field/range.range_field.873
falcon_data/0/range_field/range.range_field.598
falcon_data/0/range_field/range.range_field.507
falcon_data/0/range_field/range.range_field.190

unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" \
  | tar -tzf - | grep -v "range_field" | head -100

unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" \
  | tar -tzf - | grep -E "docids|\.meta$|creative_id|relevance_softltpacer" | head -30

# Step 1: 一次性把 tar.gz 抽到 /tmp（只一个文件，比全解数据小得多）
unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" > /tmp/falcon_data.tar.gz
ls -lh /tmp/falcon_data.tar.gz   # 看大小

# Step 2: 总文件数（知道规模）
tar -tzf /tmp/falcon_data.tar.gz | wc -l

# Step 3: 只看目录结构（过滤以 / 结尾的目录项，秒出）
tar -tzf /tmp/falcon_data.tar.gz | grep '/$' | head -50

# Step 4: 找 docids / meta / creative_id / relevance_softltpacer
tar -tzf /tmp/falcon_data.tar.gz | grep -E "docids|\.meta$|creative_id|relevance_softltpacer" | head -30

unzip "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" -d /tmp/
ls -lh /tmp/1kw_64dim_test_data/falcon_data.tar.gz

 unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" \
>   | tar -tzf - | grep -E "docids|\.meta$|creative_id|relevance_softltpacer" | head -30
falcon_data/0/creative_id/
falcon_data/0/creative_id/attachment.creative_id.0

unzip -p "1kw_64dim_test_data.zip" "1kw_64dim_test_data/falcon_data.tar.gz" | tar -tzf - | grep '/$'
