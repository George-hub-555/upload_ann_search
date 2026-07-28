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
