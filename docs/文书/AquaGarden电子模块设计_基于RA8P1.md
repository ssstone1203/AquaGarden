# AquaGarden电子模块设计(基于RA8P1)

## 供电

供电分成几路：

1. 电源->12V稳压模块->水泵驱动模块
2. 电源->[电源模块3.3v 5v 12v多路输出电压转换模块DC-DC12V转3.3v 5v 12v-tmall.com天猫](https://detail.tmall.com/item.htm?from=cart&id=751987936477&mi_id=0000cdmfa13m1fitLaTli1PcmOEdwcrpQ437H-IHfxOdMeg&spm=a1z0d.6639537%2F202410.item.d751987936477.f4c07484PMAOXX&upStreamPrice=871) ->3.3V->RA8P1/水温sensor/温度sensor供电
3. 电源->[电源模块3.3v 5v 12v多路输出电压转换模块DC-DC12V转3.3v 5v 12v-tmall.com天猫](https://detail.tmall.com/item.htm?from=cart&id=751987936477&mi_id=0000cdmfa13m1fitLaTli1PcmOEdwcrpQ437H-IHfxOdMeg&spm=a1z0d.6639537%2F202410.item.d751987936477.f4c07484PMAOXX&upStreamPrice=871)->5V->TDS sensor/土壤sensor

用[监控电源12V一分二电源线一分四 八电源线 DC5.5*2.1纯铜拖线分线-淘宝网](https://item.taobao.com/item.htm?ali_refid=a3_420434_1006%3A1683153558%3AH%3A%2BWGSGUyCZdtmkmAx%2BuiLjw%3D%3D%3A157f816ae1bef008067245e0f22a7d80&ali_trackid=283_157f816ae1bef008067245e0f22a7d80&id=888896710444&mi_id=0000f0EKoCA6Fh7iG2yzGNPOYx6TAug6Sz9ogSNa9BLJSoU&mm_sceneid=1_0_3887403986_0&priceTId=214784f517788340466051839e13ca&skuId=5900827114741&spm=a21n57.1.hoverItem.13&utparam={"aplus_abtest"%3A"780032b3b6cce8fbe9d2a029476b53d3"}&xxc=ad_ztc)连接，还需要[DC直流电源插座 5.5*2.1/5.5*2.5mm 免焊公母转接头 转接线端子-tmall.com天猫](https://detail.tmall.com/item.htm?abbucket=8&id=746873617875&mi_id=0000lOz0iJMTo3GT6gWhc9NHKMJY5HF0nd711QJZnsI1_yE&ns=1&priceTId=213e075117788350828887358e1018&skuId=5319920078678&spm=a21n57.1.hoverItem.4&utparam={"aplus_abtest"%3A"4a1fd5af98a94268ea9e44cd7bed6d6c"}&xxc=taobaoSearch)把DC转成接线端子

## 水泵驱动

水泵本身接口是DC接口

驱动模块：[【麦德斯】DRV8871模块 单路H桥直流电机驱动板 3.6A低功耗宽电压-淘宝网](https://item.taobao.com/item.htm?abbucket=8&id=810931045125&mi_id=0000ms-_Mto2HURDNtlZ7k5j82BWm_QEQn6-WdoCyKzyhhY&ns=1&skuId=5505556361680&spm=a21n57.1.item.11&utparam={"aplus_abtest"%3A"87b1f40e537dd77a6c79ecea43dd2dfc"}&xxc=taobaoSearch)，输出接口是接线端子，用[DC直流电源插座 5.5*2.1/5.5*2.5mm 免焊公母转接头 转接线端子-tmall.com天猫](https://detail.tmall.com/item.htm?abbucket=8&id=746873617875&mi_id=0000lOz0iJMTo3GT6gWhc9NHKMJY5HF0nd711QJZnsI1_yE&ns=1&priceTId=213e075117788350828887358e1018&skuId=5319920078678&spm=a21n57.1.hoverItem.4&utparam={"aplus_abtest"%3A"4a1fd5af98a94268ea9e44cd7bed6d6c"}&xxc=taobaoSearch)把接线端子转成DC母

## 台灯

[USB三色报警指示灯可编程开发485串口通信协议蜂鸣器三色灯报警灯-淘宝网](https://item.taobao.com/item.htm?abbucket=8&id=825580988569&mi_id=00009EyJORXoaB2Bc9LHkqkd_dyoH0q_aNay6D81Cesoa4s&ns=1&priceTId=215041ed17788363613486113e1b0b&skuId=5714386067147&spm=a21n57.1.hoverItem.4&utparam={"aplus_abtest"%3A"abad1a25dcd29bebd29b475b9fc7fecd"}&xxc=taobaoSearch)，使用USB串口控制颜色和亮度。需要[typec转USB3.0转接头OTG转换器tpc适用华为接口手机笔记本电脑通用连接U盘鼠标键盘苹果15充电PD数据线-tmall.com天猫](https://detail.tmall.com/item.htm?abbucket=8&id=794805643230&mi_id=0000UR4Tn-FKQUs_5-DCV6W57t3GN3yeOSwi2pnFDQEIHi4&ns=1&priceTId=213e078e17788471468584435e1083&skuId=5423672944946&spm=a21n57.1.hoverItem.3&utparam={"aplus_abtest"%3A"01be5debd3a02210d2beaf6a0d4756c9"}&xxc=taobaoSearch)适配RA8P1的typec口

## 购物清单

| 物料                  | 购买链接                                                     |
| --------------------- | ------------------------------------------------------------ |
| 转3.3V/5V多路输出模块 | [电源模块3.3v 5v 12v多路输出电压转换模块DC-DC12V转3.3v 5v 12v-tmall.com天猫](https://detail.tmall.com/item.htm?from=cart&id=751987936477&mi_id=0000cdmfa13m1fitLaTli1PcmOEdwcrpQ437H-IHfxOdMeg&spm=a1z0d.6639537%2F202410.item.d751987936477.f4c07484PMAOXX&upStreamPrice=871) |
| 水泵驱动              | [【麦德斯】DRV8871模块 单路H桥直流电机驱动板 3.6A低功耗宽电压-淘宝网](https://item.taobao.com/item.htm?abbucket=8&id=810931045125&mi_id=0000ms-_Mto2HURDNtlZ7k5j82BWm_QEQn6-WdoCyKzyhhY&ns=1&skuId=5505556361680&spm=a21n57.1.item.11&utparam={"aplus_abtest"%3A"87b1f40e537dd77a6c79ecea43dd2dfc"}&xxc=taobaoSearch) |
| DC一分二线材          | [监控电源12V一分二电源线一分四 八电源线 DC5.5*2.1纯铜拖线分线-淘宝网](https://item.taobao.com/item.htm?ali_refid=a3_420434_1006%3A1683153558%3AH%3A%2BWGSGUyCZdtmkmAx%2BuiLjw%3D%3D%3A157f816ae1bef008067245e0f22a7d80&ali_trackid=283_157f816ae1bef008067245e0f22a7d80&id=888896710444&mi_id=0000f0EKoCA6Fh7iG2yzGNPOYx6TAug6Sz9ogSNa9BLJSoU&mm_sceneid=1_0_3887403986_0&priceTId=214784f517788340466051839e13ca&skuId=5900827114741&spm=a21n57.1.hoverItem.13&utparam={"aplus_abtest"%3A"780032b3b6cce8fbe9d2a029476b53d3"}&xxc=ad_ztc) |
| 接线端子转DC母        | [DC直流电源插座 5.5*2.1/5.5*2.5mm 免焊公母转接头 转接线端子-tmall.com天猫](https://detail.tmall.com/item.htm?abbucket=8&id=746873617875&mi_id=0000lOz0iJMTo3GT6gWhc9NHKMJY5HF0nd711QJZnsI1_yE&ns=1&priceTId=213e075117788350828887358e1018&skuId=5319920078678&spm=a21n57.1.hoverItem.4&utparam={"aplus_abtest"%3A"4a1fd5af98a94268ea9e44cd7bed6d6c"}&xxc=taobaoSearch) |
| 可编程灯              | [USB三色报警指示灯可编程开发485串口通信协议蜂鸣器三色灯报警灯-淘宝网](https://item.taobao.com/item.htm?abbucket=8&id=825580988569&mi_id=00009EyJORXoaB2Bc9LHkqkd_dyoH0q_aNay6D81Cesoa4s&ns=1&priceTId=215041ed17788363613486113e1b0b&skuId=5714386067147&spm=a21n57.1.hoverItem.4&utparam={"aplus_abtest"%3A"abad1a25dcd29bebd29b475b9fc7fecd"}&xxc=taobaoSearch) |
| USB转typec            | [typec转USB3.0转接头OTG转换器tpc适用华为接口手机笔记本电脑通用连接U盘鼠标键盘苹果15充电PD数据线-tmall.com天猫](https://detail.tmall.com/item.htm?abbucket=8&id=794805643230&mi_id=0000UR4Tn-FKQUs_5-DCV6W57t3GN3yeOSwi2pnFDQEIHi4&ns=1&priceTId=213e078e17788471468584435e1083&skuId=5423672944946&spm=a21n57.1.hoverItem.3&utparam={"aplus_abtest"%3A"01be5debd3a02210d2beaf6a0d4756c9"}&xxc=taobaoSearch) |

