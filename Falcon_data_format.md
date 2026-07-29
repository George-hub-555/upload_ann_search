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


ZIP=/opt/huawei/data2/l00856060/1kw_64dim_test_data.zip

unzip -l "$ZIP"
Archive:  /opt/huawei/data2/l00856060/1kw_64dim_test_data.zip
  Length      Date    Time    Name
---------  ---------- -----   ----
        0  07-15-2026 15:56   1kw_64dim_test_data/
  8655469  02-27-2026 11:59   1kw_64dim_test_data/index_model_relevance_softLtrAfmConditionv1
264765544  02-28-2026 16:32   1kw_64dim_test_data/conditionv1_3500_one_tier_log_form.txt
10172119693  02-27-2026 12:00   1kw_64dim_test_data/falcon_data.tar.gz
174862031  02-28-2026 17:32   1kw_64dim_test_data/falcon_request_new.txt
175715191  02-28-2026 17:19   1kw_64dim_test_data/falcon_request.txt

unzip -p "$ZIP" \
  1kw_64dim_test_data/falcon_data.tar.gz \
  | tar -tzvf - \
  | head -200
**#output**
drwx------ root/root         0 2026-02-05 13:23 falcon_data/
drwx------ root/root         0 2026-02-05 13:49 falcon_data/0/
drwx------ root/root         0 2026-02-05 13:24 falcon_data/0/range_field/
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.204
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.565
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.523
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.717
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.686
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.820
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.17
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.156
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.450
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.567
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.952
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.329
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.65
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.101
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.851
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.415
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.316
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.732
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.319
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.107
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.124
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.349
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.867
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.189
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.264
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.248
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.591
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.594
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.615
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.861
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.501
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.656
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.202
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.344
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.988
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.723
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.51
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.961
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.903
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.82
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.452
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.886
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.477
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.873
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.598
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.507
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.190
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.169
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.641
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.739
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.949
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.27
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.28
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.790
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.426
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.429
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.262
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.638
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.59
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.314
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.305
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.537
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.289
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.996
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.659
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.254
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.770
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.323
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.369
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.554
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.391
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.859
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.367
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.693
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.880
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.493
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.330
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.707
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.25
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.628
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.434
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.958
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.964
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.11
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.654
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.603
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.495
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.744
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.709
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.66
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.378
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.528
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.749
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.596
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.937
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.327
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.222
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.963
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.70
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.79
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.449
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.376
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.730
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.279
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.993
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.12
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.49
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.602
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.64
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.650
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.558
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.123
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.257
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.724
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.824
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.813
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.68
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.242
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.611
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.640
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.114
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.532
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.406
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.237
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.772
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.312
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.751
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.738
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.115
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.410
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.238
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.983
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.798
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.57
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.485
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.786
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.589
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.525
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.979
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.816
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.163
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.364
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.940
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.106
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.76
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.276
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.573
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.909
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.437
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.210
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.492
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.355
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.366
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.832
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.199
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.874
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.167
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.1
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.463
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.764
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.186
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.653
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.178
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.705
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.613
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.424
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.137
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.547
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.833
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.71
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.99
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.604
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.48
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.411
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.835
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.147
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.72
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.921
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.895
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.716
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.793
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.175
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.518
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.637
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.292
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.546
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.846
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.733
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.783
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.511
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.915
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.277
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.310
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.188
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.422
-rw------- root/root    120004 2026-02-05 13:23 falcon_data/0/range_field/range.range_field.200
-rw------- root/root    120004 2026-02-05 13:24 falcon_data/0/range_field/range.range_field.584

unzip -p "$ZIP" \
  1kw_64dim_test_data/falcon_data.tar.gz \
  | tar -tzvf - \
  | head -200
**#output**


unzip -p "$ZIP" \
  1kw_64dim_test_data/conditionv1_3500_one_tier_log_form.txt \
  | head -20
**#output**

unzip -p "$ZIP" \
  1kw_64dim_test_data/falcon_request_new.txt \
  | head -5

unzip -p "$ZIP" \
  1kw_64dim_test_data/falcon_request.txt \
  | head -5

unzip -l "$ZIP" \
  1kw_64dim_test_data/index_model_relevance_softLtrAfmConditionv1

unzip -p "$ZIP" \
  1kw_64dim_test_data/index_model_relevance_softLtrAfmConditionv1 \
  | head -c 64 | xxd -g 1
