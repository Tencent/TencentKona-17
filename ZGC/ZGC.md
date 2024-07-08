# 1、java程序(使用corretto21)

```java
public class CMSGCExample {
    public static void main(String[] args) {
        List<byte[]> memoryHog = new ArrayList<>();
        int k = 1;
        while (true) {
            for (int i = 0; i < 1000; i++) {
                // 分配1MB的数组
                byte[] array = new byte[1024 * 1024];
                memoryHog.add(array);
            }
            System.out.println("第" + k++ + "分配1M内存" + "List 大小：" + memoryHog.size());
            // 模拟内存使用情况
            if (memoryHog.size() > 1000) {
                memoryHog.subList(0, 500).clear();
            }

            try {
                // 暂停一段时间，模拟应用程序的其他工作
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
```

# 2、java命令

```
-XX:+UseZGC   
-Xlog:gc*:file=gc.log  
```

命令解释：

```angular2html
-XX:+UseZGC             //启用Z垃圾收集器（ZGC）
-Xlog:gc*:file=gc.log  //启用GC日志记录，并将所有GC相关的信息记录到文件gc.log中
```

# 3、运行结果

![img.png](image/img.png)

# 4、GC日志部分分析

<details>
    <summary>gc.log日志</summary>

```log
[0.013s][info][gc,init] Initializing The Z Garbage Collector
[0.014s][info][gc,init] Version: 21.0.3+9-LTS (release)
[0.014s][info][gc,init] Using legacy single-generation mode
[0.014s][info][gc,init] NUMA Support: Disabled
[0.014s][info][gc,init] CPUs: 8 total, 8 available
[0.014s][info][gc,init] Memory: 16281M
[0.014s][info][gc,init] Large Page Support: Disabled
[0.014s][info][gc,init] GC Workers: 2 (dynamic)
[0.015s][info][gc,init] Address Space Type: Contiguous/Unrestricted/Complete
[0.015s][info][gc,init] Address Space Size: 65152M x 3 = 195456M
[0.015s][info][gc,init] Min Capacity: 8M
[0.015s][info][gc,init] Initial Capacity: 256M
[0.015s][info][gc,init] Max Capacity: 4072M
[0.015s][info][gc,init] Medium Page Size: 32M
[0.015s][info][gc,init] Pre-touch: Disabled
[0.015s][info][gc,init] Uncommit: Enabled
[0.015s][info][gc,init] Uncommit Delay: 300s
[0.027s][info][gc,init] Runtime Workers: 5
[0.028s][info][gc     ] Using The Z Garbage Collector
[0.039s][info][gc,metaspace] CDS archive(s) mapped at: [0x0000019a98000000-0x0000019a98c90000-0x0000019a98c90000), size 13172736, SharedBaseAddress: 0x0000019a98000000, ArchiveRelocationMode: 1.
[0.039s][info][gc,metaspace] Compressed class space mapped at: 0x0000019a99000000-0x0000019ad9000000, reserved size: 1073741824
[0.039s][info][gc,metaspace] Narrow klass base: 0x0000019a98000000, Narrow klass shift: 0, Narrow klass range: 0x100000000
[0.460s][info][gc,start    ] GC(0) Garbage Collection (Warmup)
[0.461s][info][gc,task     ] GC(0) Using 2 workers
[0.461s][info][gc,phases   ] GC(0) Pause Mark Start 0.007ms
[0.475s][info][gc,phases   ] GC(0) Concurrent Mark 14.514ms
[0.476s][info][gc,phases   ] GC(0) Pause Mark End 0.018ms
[0.476s][info][gc,phases   ] GC(0) Concurrent Mark Free 0.001ms
[0.476s][info][gc,phases   ] GC(0) Concurrent Process Non-Strong References 0.650ms
[0.476s][info][gc,phases   ] GC(0) Concurrent Reset Relocation Set 0.001ms
[0.485s][info][gc,phases   ] GC(0) Concurrent Select Relocation Set 9.009ms
[0.486s][info][gc,phases   ] GC(0) Pause Relocate Start 0.013ms
[0.487s][info][gc,phases   ] GC(0) Concurrent Relocate 0.810ms
[0.487s][info][gc,load     ] GC(0) Load: 0.00/0.00/0.00
[0.487s][info][gc,mmu      ] GC(0) MMU: 2ms/99.1%, 5ms/99.6%, 10ms/99.8%, 20ms/99.8%, 50ms/99.9%, 100ms/100.0%
[0.487s][info][gc,marking  ] GC(0) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[0.487s][info][gc,marking  ] GC(0) Mark Stack Usage: 32M
[0.487s][info][gc,nmethod  ] GC(0) NMethods: 289 registered, 41 unregistered
[0.487s][info][gc,metaspace] GC(0) Metaspace: 0M used, 0M committed, 1088M reserved
[0.487s][info][gc,ref      ] GC(0) Soft: 54 encountered, 0 discovered, 0 enqueued
[0.487s][info][gc,ref      ] GC(0) Weak: 171 encountered, 48 discovered, 45 enqueued
[0.487s][info][gc,ref      ] GC(0) Final: 0 encountered, 0 discovered, 0 enqueued
[0.487s][info][gc,ref      ] GC(0) Phantom: 187 encountered, 90 discovered, 15 enqueued
[0.487s][info][gc,reloc    ] GC(0) Small Pages: 6 / 12M, Empty: 0M, Relocated: 0M, In-Place: 0
[0.487s][info][gc,reloc    ] GC(0) Medium Pages: 14 / 448M, Empty: 0M, Relocated: 0M, In-Place: 0
[0.487s][info][gc,reloc    ] GC(0) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[0.487s][info][gc,reloc    ] GC(0) Forwarding Usage: 0M
[0.487s][info][gc,heap     ] GC(0) Min Capacity: 8M(0%)
[0.487s][info][gc,heap     ] GC(0) Max Capacity: 4072M(100%)
[0.487s][info][gc,heap     ] GC(0) Soft Max Capacity: 4072M(100%)
[0.487s][info][gc,heap     ] GC(0)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[0.487s][info][gc,heap     ] GC(0)  Capacity:      480M (12%)         512M (13%)         544M (13%)         544M (13%)         544M (13%)         480M (12%)    
[0.487s][info][gc,heap     ] GC(0)      Free:     3612M (89%)        3580M (88%)        3546M (87%)        3548M (87%)        3612M (89%)        3542M (87%)    
[0.487s][info][gc,heap     ] GC(0)      Used:      460M (11%)         492M (12%)         526M (13%)         524M (13%)         530M (13%)         460M (11%)    
[0.487s][info][gc,heap     ] GC(0)      Live:         -               437M (11%)         437M (11%)         437M (11%)            -                  -          
[0.487s][info][gc,heap     ] GC(0) Allocated:         -                32M (1%)           66M (2%)           67M (2%)             -                  -          
[0.487s][info][gc,heap     ] GC(0)   Garbage:         -                22M (1%)           22M (1%)           18M (0%)             -                  -          
[0.487s][info][gc,heap     ] GC(0) Reclaimed:         -                  -                 0M (0%)            3M (0%)             -                  -          
[0.487s][info][gc          ] GC(0) Garbage Collection (Warmup) 460M(11%)->524M(13%)
[0.761s][info][gc,start    ] GC(1) Garbage Collection (Warmup)
[0.761s][info][gc,task     ] GC(1) Using 2 workers
[0.761s][info][gc,phases   ] GC(1) Pause Mark Start 0.008ms
[0.777s][info][gc,phases   ] GC(1) Concurrent Mark 15.676ms
[0.777s][info][gc,phases   ] GC(1) Pause Mark End 0.014ms
[0.777s][info][gc,phases   ] GC(1) Concurrent Mark Free 0.001ms
[0.778s][info][gc,phases   ] GC(1) Concurrent Process Non-Strong References 0.641ms
[0.778s][info][gc,phases   ] GC(1) Concurrent Reset Relocation Set 0.001ms
[0.780s][info][gc,phases   ] GC(1) Concurrent Select Relocation Set 2.204ms
[0.780s][info][gc,phases   ] GC(1) Pause Relocate Start 0.006ms
[0.782s][info][gc,phases   ] GC(1) Concurrent Relocate 2.261ms
[0.782s][info][gc,load     ] GC(1) Load: 0.00/0.00/0.00
[0.783s][info][gc,mmu      ] GC(1) MMU: 2ms/99.1%, 5ms/99.6%, 10ms/99.8%, 20ms/99.8%, 50ms/99.9%, 100ms/100.0%
[0.783s][info][gc,marking  ] GC(1) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[0.783s][info][gc,marking  ] GC(1) Mark Stack Usage: 32M
[0.783s][info][gc,nmethod  ] GC(1) NMethods: 403 registered, 51 unregistered
[0.783s][info][gc,metaspace] GC(1) Metaspace: 1M used, 1M committed, 1088M reserved
[0.783s][info][gc,ref      ] GC(1) Soft: 116 encountered, 0 discovered, 0 enqueued
[0.783s][info][gc,ref      ] GC(1) Weak: 235 encountered, 88 discovered, 30 enqueued
[0.783s][info][gc,ref      ] GC(1) Final: 0 encountered, 0 discovered, 0 enqueued
[0.783s][info][gc,ref      ] GC(1) Phantom: 181 encountered, 88 discovered, 9 enqueued
[0.783s][info][gc,reloc    ] GC(1) Small Pages: 8 / 16M, Empty: 0M, Relocated: 2M, In-Place: 0
[0.783s][info][gc,reloc    ] GC(1) Medium Pages: 29 / 928M, Empty: 0M, Relocated: 0M, In-Place: 0
[0.783s][info][gc,reloc    ] GC(1) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[0.783s][info][gc,reloc    ] GC(1) Forwarding Usage: 0M
[0.783s][info][gc,heap     ] GC(1) Min Capacity: 8M(0%)
[0.783s][info][gc,heap     ] GC(1) Max Capacity: 4072M(100%)
[0.783s][info][gc,heap     ] GC(1) Soft Max Capacity: 4072M(100%)
[0.783s][info][gc,heap     ] GC(1)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[0.783s][info][gc,heap     ] GC(1)  Capacity:      960M (24%)         992M (24%)         992M (24%)        1024M (25%)        1024M (25%)         960M (24%)    
[0.783s][info][gc,heap     ] GC(1)      Free:     3128M (77%)        3096M (76%)        3094M (76%)        3070M (75%)        3128M (77%)        3066M (75%)    
[0.783s][info][gc,heap     ] GC(1)      Used:      944M (23%)         976M (24%)         978M (24%)        1002M (25%)        1006M (25%)         944M (23%)    
[0.783s][info][gc,heap     ] GC(1)      Live:         -               903M (22%)         903M (22%)         903M (22%)            -                  -          
[0.783s][info][gc,heap     ] GC(1) Allocated:         -                32M (1%)           34M (1%)           67M (2%)             -                  -          
[0.783s][info][gc,heap     ] GC(1)   Garbage:         -                40M (1%)           40M (1%)           30M (1%)             -                  -          
[0.783s][info][gc,heap     ] GC(1) Reclaimed:         -                  -                 0M (0%)            9M (0%)             -                  -          
[0.783s][info][gc          ] GC(1) Garbage Collection (Warmup) 944M(23%)->1002M(25%)
[1.061s][info][gc,start    ] GC(2) Garbage Collection (Warmup)
[1.061s][info][gc,task     ] GC(2) Using 2 workers
[1.061s][info][gc,phases   ] GC(2) Pause Mark Start 0.007ms
[1.077s][info][gc,phases   ] GC(2) Concurrent Mark 15.686ms
[1.077s][info][gc,phases   ] GC(2) Pause Mark End 0.016ms
[1.077s][info][gc,phases   ] GC(2) Concurrent Mark Free 0.001ms
[1.078s][info][gc,phases   ] GC(2) Concurrent Process Non-Strong References 0.928ms
[1.078s][info][gc,phases   ] GC(2) Concurrent Reset Relocation Set 0.002ms
[1.081s][info][gc,phases   ] GC(2) Concurrent Select Relocation Set 2.687ms
[1.081s][info][gc,phases   ] GC(2) Pause Relocate Start 0.014ms
[1.082s][info][gc,phases   ] GC(2) Concurrent Relocate 1.172ms
[1.082s][info][gc,load     ] GC(2) Load: 0.00/0.00/0.00
[1.082s][info][gc,mmu      ] GC(2) MMU: 2ms/99.1%, 5ms/99.4%, 10ms/99.7%, 20ms/99.8%, 50ms/99.9%, 100ms/100.0%
[1.082s][info][gc,marking  ] GC(2) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[1.082s][info][gc,marking  ] GC(2) Mark Stack Usage: 32M
[1.082s][info][gc,nmethod  ] GC(2) NMethods: 403 registered, 52 unregistered
[1.082s][info][gc,metaspace] GC(2) Metaspace: 1M used, 1M committed, 1088M reserved
[1.082s][info][gc,ref      ] GC(2) Soft: 116 encountered, 0 discovered, 0 enqueued
[1.082s][info][gc,ref      ] GC(2) Weak: 205 encountered, 120 discovered, 0 enqueued
[1.082s][info][gc,ref      ] GC(2) Final: 0 encountered, 0 discovered, 0 enqueued
[1.082s][info][gc,ref      ] GC(2) Phantom: 172 encountered, 161 discovered, 0 enqueued
[1.082s][info][gc,reloc    ] GC(2) Small Pages: 5 / 10M, Empty: 0M, Relocated: 1M, In-Place: 0
[1.083s][info][gc,reloc    ] GC(2) Medium Pages: 39 / 1248M, Empty: 0M, Relocated: 0M, In-Place: 0
[1.083s][info][gc,reloc    ] GC(2) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[1.083s][info][gc,reloc    ] GC(2) Forwarding Usage: 0M
[1.083s][info][gc,heap     ] GC(2) Min Capacity: 8M(0%)
[1.083s][info][gc,heap     ] GC(2) Max Capacity: 4072M(100%)
[1.083s][info][gc,heap     ] GC(2) Soft Max Capacity: 4072M(100%)
[1.083s][info][gc,heap     ] GC(2)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[1.083s][info][gc,heap     ] GC(2)  Capacity:     1280M (31%)        1312M (32%)        1312M (32%)        1312M (32%)        1312M (32%)        1280M (31%)    
[1.083s][info][gc,heap     ] GC(2)      Free:     2814M (69%)        2782M (68%)        2782M (68%)        2782M (68%)        2814M (69%)        2776M (68%)    
[1.083s][info][gc,heap     ] GC(2)      Used:     1258M (31%)        1290M (32%)        1290M (32%)        1290M (32%)        1296M (32%)        1258M (31%)    
[1.083s][info][gc,heap     ] GC(2)      Live:         -              1207M (30%)        1207M (30%)        1207M (30%)            -                  -          
[1.083s][info][gc,heap     ] GC(2) Allocated:         -                32M (1%)           32M (1%)           33M (1%)             -                  -          
[1.083s][info][gc,heap     ] GC(2)   Garbage:         -                50M (1%)           50M (1%)           48M (1%)             -                  -          
[1.083s][info][gc,heap     ] GC(2) Reclaimed:         -                  -                 0M (0%)            1M (0%)             -                  -          
[1.083s][info][gc          ] GC(2) Garbage Collection (Warmup) 1258M(31%)->1290M(32%)
[2.666s][info][gc,start    ] GC(3) Garbage Collection (Proactive)
[2.666s][info][gc,task     ] GC(3) Using 2 workers
[2.666s][info][gc,phases   ] GC(3) Pause Mark Start 0.009ms
[2.682s][info][gc,phases   ] GC(3) Concurrent Mark 15.647ms
[2.682s][info][gc,phases   ] GC(3) Pause Mark End 0.014ms
[2.682s][info][gc,phases   ] GC(3) Concurrent Mark Free 0.001ms
[2.683s][info][gc,phases   ] GC(3) Concurrent Process Non-Strong References 0.866ms
[2.683s][info][gc,phases   ] GC(3) Concurrent Reset Relocation Set 0.001ms
[2.686s][info][gc,phases   ] GC(3) Concurrent Select Relocation Set 2.842ms
[2.686s][info][gc,phases   ] GC(3) Pause Relocate Start 0.007ms
[2.689s][info][gc,phases   ] GC(3) Concurrent Relocate 2.599ms
[2.689s][info][gc,load     ] GC(3) Load: 0.00/0.00/0.00
[2.689s][info][gc,mmu      ] GC(3) MMU: 2ms/99.1%, 5ms/99.4%, 10ms/99.7%, 20ms/99.8%, 50ms/99.9%, 100ms/100.0%
[2.689s][info][gc,marking  ] GC(3) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[2.689s][info][gc,marking  ] GC(3) Mark Stack Usage: 32M
[2.689s][info][gc,nmethod  ] GC(3) NMethods: 403 registered, 52 unregistered
[2.689s][info][gc,metaspace] GC(3) Metaspace: 1M used, 1M committed, 1088M reserved
[2.689s][info][gc,ref      ] GC(3) Soft: 116 encountered, 0 discovered, 0 enqueued
[2.689s][info][gc,ref      ] GC(3) Weak: 205 encountered, 9 discovered, 0 enqueued
[2.689s][info][gc,ref      ] GC(3) Final: 0 encountered, 0 discovered, 0 enqueued
[2.689s][info][gc,ref      ] GC(3) Phantom: 172 encountered, 146 discovered, 0 enqueued
[2.689s][info][gc,reloc    ] GC(3) Small Pages: 5 / 10M, Empty: 0M, Relocated: 1M, In-Place: 0
[2.689s][info][gc,reloc    ] GC(3) Medium Pages: 106 / 3392M, Empty: 992M, Relocated: 11M, In-Place: 0
[2.689s][info][gc,reloc    ] GC(3) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[2.689s][info][gc,reloc    ] GC(3) Forwarding Usage: 0M
[2.689s][info][gc,heap     ] GC(3) Min Capacity: 8M(0%)
[2.689s][info][gc,heap     ] GC(3) Max Capacity: 4072M(100%)
[2.689s][info][gc,heap     ] GC(3) Soft Max Capacity: 4072M(100%)
[2.689s][info][gc,heap     ] GC(3)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[2.689s][info][gc,heap     ] GC(3)  Capacity:     3424M (84%)        3456M (85%)        3456M (85%)        3456M (85%)        3456M (85%)        3424M (84%)    
[2.689s][info][gc,heap     ] GC(3)      Free:      670M (16%)         638M (16%)        1630M (40%)        1632M (40%)        1632M (40%)         638M (16%)    
[2.689s][info][gc,heap     ] GC(3)      Used:     3402M (84%)        3434M (84%)        2442M (60%)        2440M (60%)        3434M (84%)        2440M (60%)    
[2.689s][info][gc,heap     ] GC(3)      Live:         -              2268M (56%)        2268M (56%)        2268M (56%)            -                  -          
[2.689s][info][gc,heap     ] GC(3) Allocated:         -                32M (1%)           32M (1%)           65M (2%)             -                  -          
[2.689s][info][gc,heap     ] GC(3)   Garbage:         -              1133M (28%)         141M (3%)          105M (3%)             -                  -          
[2.689s][info][gc,heap     ] GC(3) Reclaimed:         -                  -               992M (24%)        1027M (25%)            -                  -          
[2.689s][info][gc          ] GC(3) Garbage Collection (Proactive) 3402M(84%)->2440M(60%)
[3.075s][info][gc,start    ] GC(4) Garbage Collection (Allocation Rate)
[3.075s][info][gc,task     ] GC(4) Using 2 workers
[3.075s][info][gc,phases   ] GC(4) Pause Mark Start 0.006ms
[3.091s][info][gc,phases   ] GC(4) Concurrent Mark 15.693ms
[3.091s][info][gc,phases   ] GC(4) Pause Mark End 0.024ms
[3.091s][info][gc,phases   ] GC(4) Concurrent Mark Free 0.001ms
[3.092s][info][gc,phases   ] GC(4) Concurrent Process Non-Strong References 0.771ms
[3.092s][info][gc,phases   ] GC(4) Concurrent Reset Relocation Set 0.001ms
[3.094s][info][gc,phases   ] GC(4) Concurrent Select Relocation Set 1.723ms
[3.094s][info][gc,phases   ] GC(4) Pause Relocate Start 0.006ms
[3.098s][info][gc,phases   ] GC(4) Concurrent Relocate 4.595ms
[3.099s][info][gc,load     ] GC(4) Load: 0.00/0.00/0.00
[3.099s][info][gc,mmu      ] GC(4) MMU: 2ms/98.8%, 5ms/99.4%, 10ms/99.7%, 20ms/99.8%, 50ms/99.9%, 100ms/100.0%
[3.099s][info][gc,marking  ] GC(4) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[3.099s][info][gc,marking  ] GC(4) Mark Stack Usage: 32M
[3.099s][info][gc,nmethod  ] GC(4) NMethods: 403 registered, 53 unregistered
[3.099s][info][gc,metaspace] GC(4) Metaspace: 1M used, 1M committed, 1088M reserved
[3.099s][info][gc,ref      ] GC(4) Soft: 116 encountered, 0 discovered, 0 enqueued
[3.099s][info][gc,ref      ] GC(4) Weak: 205 encountered, 8 discovered, 0 enqueued
[3.099s][info][gc,ref      ] GC(4) Final: 0 encountered, 0 discovered, 0 enqueued
[3.099s][info][gc,ref      ] GC(4) Phantom: 172 encountered, 136 discovered, 0 enqueued
[3.099s][info][gc,reloc    ] GC(4) Small Pages: 4 / 8M, Empty: 0M, Relocated: 1M, In-Place: 0
[3.099s][info][gc,reloc    ] GC(4) Medium Pages: 114 / 3648M, Empty: 512M, Relocated: 19M, In-Place: 0
[3.099s][info][gc,reloc    ] GC(4) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[3.099s][info][gc,reloc    ] GC(4) Forwarding Usage: 0M
[3.099s][info][gc,heap     ] GC(4) Min Capacity: 8M(0%)
[3.099s][info][gc,heap     ] GC(4) Max Capacity: 4072M(100%)
[3.099s][info][gc,heap     ] GC(4) Soft Max Capacity: 4072M(100%)
[3.099s][info][gc,heap     ] GC(4)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[3.099s][info][gc,heap     ] GC(4)  Capacity:     3680M (90%)        3712M (91%)        3712M (91%)        3712M (91%)        3712M (91%)        3680M (90%)    
[3.099s][info][gc,heap     ] GC(4)      Free:      416M (10%)         384M (9%)          896M (22%)         928M (23%)         928M (23%)         384M (9%)     
[3.099s][info][gc,heap     ] GC(4)      Used:     3656M (90%)        3688M (91%)        3176M (78%)        3144M (77%)        3688M (91%)        3144M (77%)    
[3.099s][info][gc,heap     ] GC(4)      Live:         -              3011M (74%)        3011M (74%)        3011M (74%)            -                  -          
[3.099s][info][gc,heap     ] GC(4) Allocated:         -                32M (1%)           32M (1%)           33M (1%)             -                  -          
[3.099s][info][gc,heap     ] GC(4)   Garbage:         -               644M (16%)         132M (3%)           98M (2%)             -                  -          
[3.099s][info][gc,heap     ] GC(4) Reclaimed:         -                  -               512M (13%)         545M (13%)            -                  -          
[3.099s][info][gc          ] GC(4) Garbage Collection (Allocation Rate) 3656M(90%)->3144M(77%)
[3.171s][info][gc,start    ] GC(5) Garbage Collection (Allocation Rate)
[3.171s][info][gc,task     ] GC(5) Using 2 workers
[3.171s][info][gc,phases   ] GC(5) Pause Mark Start 0.005ms
[3.187s][info][gc,phases   ] GC(5) Concurrent Mark 15.973ms
[3.187s][info][gc,phases   ] GC(5) Pause Mark End 0.011ms
[3.187s][info][gc,phases   ] GC(5) Concurrent Mark Free 0.001ms
[3.188s][info][gc,phases   ] GC(5) Concurrent Process Non-Strong References 0.723ms
[3.188s][info][gc,phases   ] GC(5) Concurrent Reset Relocation Set 0.001ms
[3.190s][info][gc,phases   ] GC(5) Concurrent Select Relocation Set 1.918ms
[3.190s][info][gc,phases   ] GC(5) Pause Relocate Start 0.005ms
[3.194s][info][gc,phases   ] GC(5) Concurrent Relocate 3.857ms
[3.194s][info][gc,load     ] GC(5) Load: 0.00/0.00/0.00
[3.194s][info][gc,mmu      ] GC(5) MMU: 2ms/98.8%, 5ms/99.4%, 10ms/99.7%, 20ms/99.8%, 50ms/99.9%, 100ms/99.9%
[3.194s][info][gc,marking  ] GC(5) Mark: 2 stripe(s), 1 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[3.194s][info][gc,marking  ] GC(5) Mark Stack Usage: 32M
[3.194s][info][gc,nmethod  ] GC(5) NMethods: 405 registered, 53 unregistered
[3.194s][info][gc,metaspace] GC(5) Metaspace: 1M used, 1M committed, 1088M reserved
[3.194s][info][gc,ref      ] GC(5) Soft: 116 encountered, 0 discovered, 0 enqueued
[3.194s][info][gc,ref      ] GC(5) Weak: 205 encountered, 59 discovered, 0 enqueued
[3.194s][info][gc,ref      ] GC(5) Final: 0 encountered, 0 discovered, 0 enqueued
[3.194s][info][gc,ref      ] GC(5) Phantom: 172 encountered, 137 discovered, 0 enqueued
[3.194s][info][gc,reloc    ] GC(5) Small Pages: 4 / 8M, Empty: 0M, Relocated: 1M, In-Place: 0
[3.194s][info][gc,reloc    ] GC(5) Medium Pages: 114 / 3648M, Empty: 480M, Relocated: 24M, In-Place: 0
[3.194s][info][gc,reloc    ] GC(5) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[3.194s][info][gc,reloc    ] GC(5) Forwarding Usage: 0M
[3.194s][info][gc,heap     ] GC(5) Min Capacity: 8M(0%)
[3.194s][info][gc,heap     ] GC(5) Max Capacity: 4072M(100%)
[3.194s][info][gc,heap     ] GC(5) Soft Max Capacity: 4072M(100%)
[3.194s][info][gc,heap     ] GC(5)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[3.194s][info][gc,heap     ] GC(5)  Capacity:     3712M (91%)        3712M (91%)        3712M (91%)        3712M (91%)        3712M (91%)        3712M (91%)    
[3.194s][info][gc,heap     ] GC(5)      Free:      416M (10%)         416M (10%)         896M (22%)         962M (24%)         962M (24%)         416M (10%)    
[3.194s][info][gc,heap     ] GC(5)      Used:     3656M (90%)        3656M (90%)        3176M (78%)        3110M (76%)        3656M (90%)        3110M (76%)    
[3.194s][info][gc,heap     ] GC(5)      Live:         -              3016M (74%)        3016M (74%)        3016M (74%)            -                  -          
[3.194s][info][gc,heap     ] GC(5) Allocated:         -                 0M (0%)            0M (0%)            0M (0%)             -                  -          
[3.194s][info][gc,heap     ] GC(5)   Garbage:         -               639M (16%)         159M (4%)           93M (2%)             -                  -          
[3.194s][info][gc,heap     ] GC(5) Reclaimed:         -                  -               480M (12%)         546M (13%)            -                  -          
[3.194s][info][gc          ] GC(5) Garbage Collection (Allocation Rate) 3656M(90%)->3110M(76%)
[3.362s][info][gc,start    ] GC(6) Garbage Collection (Allocation Rate)
[3.362s][info][gc,task     ] GC(6) Using 2 workers
[3.362s][info][gc,phases   ] GC(6) Pause Mark Start 0.013ms
[3.378s][info][gc,phases   ] GC(6) Concurrent Mark 15.330ms
[3.378s][info][gc,phases   ] GC(6) Pause Mark End 0.014ms
[3.378s][info][gc,phases   ] GC(6) Concurrent Mark Free 0.001ms
[3.379s][info][gc,phases   ] GC(6) Concurrent Process Non-Strong References 1.085ms
[3.379s][info][gc,phases   ] GC(6) Concurrent Reset Relocation Set 0.001ms
[3.382s][info][gc,phases   ] GC(6) Concurrent Select Relocation Set 2.535ms
[3.382s][info][gc,phases   ] GC(6) Pause Relocate Start 0.008ms
[3.382s][info][gc,phases   ] GC(6) Concurrent Relocate 0.073ms
[3.382s][info][gc,load     ] GC(6) Load: 0.00/0.00/0.00
[3.382s][info][gc,mmu      ] GC(6) MMU: 2ms/98.8%, 5ms/99.4%, 10ms/99.7%, 20ms/99.8%, 50ms/99.9%, 100ms/99.9%
[3.382s][info][gc,marking  ] GC(6) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[3.382s][info][gc,marking  ] GC(6) Mark Stack Usage: 32M
[3.382s][info][gc,nmethod  ] GC(6) NMethods: 405 registered, 53 unregistered
[3.382s][info][gc,metaspace] GC(6) Metaspace: 1M used, 1M committed, 1088M reserved
[3.382s][info][gc,ref      ] GC(6) Soft: 116 encountered, 0 discovered, 0 enqueued
[3.382s][info][gc,ref      ] GC(6) Weak: 205 encountered, 8 discovered, 0 enqueued
[3.382s][info][gc,ref      ] GC(6) Final: 0 encountered, 0 discovered, 0 enqueued
[3.382s][info][gc,ref      ] GC(6) Phantom: 172 encountered, 139 discovered, 0 enqueued
[3.382s][info][gc,reloc    ] GC(6) Small Pages: 3 / 6M, Empty: 0M, Relocated: 0M, In-Place: 0
[3.382s][info][gc,reloc    ] GC(6) Medium Pages: 118 / 3776M, Empty: 0M, Relocated: 0M, In-Place: 0
[3.382s][info][gc,reloc    ] GC(6) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[3.382s][info][gc,reloc    ] GC(6) Forwarding Usage: 0M
[3.382s][info][gc,heap     ] GC(6) Min Capacity: 8M(0%)
[3.382s][info][gc,heap     ] GC(6) Max Capacity: 4072M(100%)
[3.382s][info][gc,heap     ] GC(6) Soft Max Capacity: 4072M(100%)
[3.382s][info][gc,heap     ] GC(6)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[3.382s][info][gc,heap     ] GC(6)  Capacity:     3808M (94%)        3840M (94%)        3840M (94%)        3840M (94%)        3840M (94%)        3808M (94%)    
[3.382s][info][gc,heap     ] GC(6)      Free:      290M (7%)          258M (6%)          258M (6%)          258M (6%)          290M (7%)          258M (6%)     
[3.382s][info][gc,heap     ] GC(6)      Used:     3782M (93%)        3814M (94%)        3814M (94%)        3814M (94%)        3814M (94%)        3782M (93%)    
[3.382s][info][gc,heap     ] GC(6)      Live:         -              3664M (90%)        3664M (90%)        3664M (90%)            -                  -          
[3.382s][info][gc,heap     ] GC(6) Allocated:         -                32M (1%)           32M (1%)           32M (1%)             -                  -          
[3.382s][info][gc,heap     ] GC(6)   Garbage:         -               117M (3%)          117M (3%)          117M (3%)             -                  -          
[3.382s][info][gc,heap     ] GC(6) Reclaimed:         -                  -                 0M (0%)            0M (0%)             -                  -          
[3.382s][info][gc          ] GC(6) Garbage Collection (Allocation Rate) 3782M(93%)->3814M(94%)
[3.489s][info][gc,start    ] GC(7) Garbage Collection (Allocation Rate)
[3.492s][info][gc,task     ] GC(7) Using 2 workers
[3.503s][info][gc,phases   ] GC(7) Pause Mark Start 4.849ms
[4.272s][info][gc,phases   ] GC(7) Concurrent Mark 766.731ms
[4.289s][info][gc,phases   ] GC(7) Pause Mark End 1.598ms
[4.289s][info][gc,phases   ] GC(7) Concurrent Mark Free 0.001ms
[4.297s][info][gc,phases   ] GC(7) Concurrent Process Non-Strong References 7.571ms
[4.297s][info][gc,phases   ] GC(7) Concurrent Reset Relocation Set 0.003ms
[4.377s][info][gc,phases   ] GC(7) Concurrent Select Relocation Set 80.626ms
[4.377s][info][gc,phases   ] GC(7) Pause Relocate Start 0.011ms
[4.378s][info][gc,phases   ] GC(7) Concurrent Relocate 0.124ms
[4.385s][info][gc,load     ] GC(7) Load: 0.00/0.00/0.00
[4.385s][info][gc,mmu      ] GC(7) MMU: 2ms/0.0%, 5ms/3.0%, 10ms/51.5%, 20ms/75.8%, 50ms/90.3%, 100ms/95.2%
[4.385s][info][gc,marking  ] GC(7) Mark: 2 stripe(s), 3 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[4.385s][info][gc,marking  ] GC(7) Mark Stack Usage: 32M
[4.385s][info][gc,nmethod  ] GC(7) NMethods: 405 registered, 53 unregistered
[4.386s][info][gc,metaspace] GC(7) Metaspace: 1M used, 1M committed, 1088M reserved
[4.386s][info][gc,ref      ] GC(7) Soft: 116 encountered, 0 discovered, 0 enqueued
[4.386s][info][gc,ref      ] GC(7) Weak: 205 encountered, 70 discovered, 0 enqueued
[4.386s][info][gc,ref      ] GC(7) Final: 0 encountered, 0 discovered, 0 enqueued
[4.386s][info][gc,ref      ] GC(7) Phantom: 172 encountered, 98 discovered, 0 enqueued
[4.386s][info][gc,reloc    ] GC(7) Small Pages: 3 / 6M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.386s][info][gc,reloc    ] GC(7) Medium Pages: 122 / 3904M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.386s][info][gc,reloc    ] GC(7) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.386s][info][gc,reloc    ] GC(7) Forwarding Usage: 0M
[4.386s][info][gc,heap     ] GC(7) Min Capacity: 8M(0%)
[4.386s][info][gc,heap     ] GC(7) Max Capacity: 4072M(100%)
[4.387s][info][gc,heap     ] GC(7) Soft Max Capacity: 4072M(100%)
[4.387s][info][gc,heap     ] GC(7)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[4.387s][info][gc,heap     ] GC(7)  Capacity:     3936M (97%)        3968M (97%)        4000M (98%)        4000M (98%)        4000M (98%)        3936M (97%)    
[4.387s][info][gc,heap     ] GC(7)      Free:      162M (4%)          130M (3%)           98M (2%)           98M (2%)          162M (4%)           98M (2%)     
[4.387s][info][gc,heap     ] GC(7)      Used:     3910M (96%)        3942M (97%)        3974M (98%)        3974M (98%)        3974M (98%)        3910M (96%)    
[4.387s][info][gc,heap     ] GC(7)      Live:         -              3759M (92%)        3759M (92%)        3759M (92%)            -                  -          
[4.387s][info][gc,heap     ] GC(7) Allocated:         -                32M (1%)           64M (2%)           64M (2%)             -                  -          
[4.387s][info][gc,heap     ] GC(7)   Garbage:         -               150M (4%)          150M (4%)          150M (4%)             -                  -          
[4.387s][info][gc,heap     ] GC(7) Reclaimed:         -                  -                 0M (0%)            0M (0%)             -                  -          
[4.387s][info][gc          ] GC(7) Garbage Collection (Allocation Rate) 3910M(96%)->3974M(98%)
[4.470s][info][gc,start    ] GC(8) Garbage Collection (Allocation Rate)
[4.470s][info][gc,task     ] GC(8) Using 2 workers
[4.471s][info][gc,phases   ] GC(8) Pause Mark Start 0.006ms
[4.511s][info][gc,phases   ] GC(8) Concurrent Mark 40.088ms
[4.512s][info][gc,phases   ] GC(8) Pause Mark End 0.010ms
[4.512s][info][gc,phases   ] GC(8) Concurrent Mark Free 0.001ms
[4.512s][info][gc,phases   ] GC(8) Concurrent Process Non-Strong References 0.866ms
[4.513s][info][gc,phases   ] GC(8) Concurrent Reset Relocation Set 0.000ms
[4.514s][info][gc,phases   ] GC(8) Concurrent Select Relocation Set 1.697ms
[4.514s][info][gc,phases   ] GC(8) Pause Relocate Start 0.005ms
[4.567s][info][gc,phases   ] GC(8) Concurrent Relocate 52.876ms
[4.567s][info][gc,load     ] GC(8) Load: 0.00/0.00/0.00
[4.567s][info][gc,mmu      ] GC(8) MMU: 2ms/0.0%, 5ms/3.0%, 10ms/51.5%, 20ms/75.8%, 50ms/90.3%, 100ms/95.2%
[4.567s][info][gc,marking  ] GC(8) Mark: 2 stripe(s), 4 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[4.567s][info][gc,marking  ] GC(8) Mark Stack Usage: 32M
[4.567s][info][gc,nmethod  ] GC(8) NMethods: 405 registered, 53 unregistered
[4.567s][info][gc,metaspace] GC(8) Metaspace: 1M used, 1M committed, 1088M reserved
[4.567s][info][gc,ref      ] GC(8) Soft: 116 encountered, 0 discovered, 0 enqueued
[4.567s][info][gc,ref      ] GC(8) Weak: 205 encountered, 9 discovered, 0 enqueued
[4.567s][info][gc,ref      ] GC(8) Final: 0 encountered, 0 discovered, 0 enqueued
[4.567s][info][gc,ref      ] GC(8) Phantom: 172 encountered, 80 discovered, 0 enqueued
[4.567s][info][gc,reloc    ] GC(8) Small Pages: 3 / 6M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.568s][info][gc,reloc    ] GC(8) Medium Pages: 125 / 4000M, Empty: 0M, Relocated: 22M, In-Place: 0
[4.568s][info][gc,reloc    ] GC(8) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.568s][info][gc,reloc    ] GC(8) Forwarding Usage: 0M
[4.568s][info][gc,heap     ] GC(8) Min Capacity: 8M(0%)
[4.568s][info][gc,heap     ] GC(8) Max Capacity: 4072M(100%)
[4.568s][info][gc,heap     ] GC(8) Soft Max Capacity: 4072M(100%)
[4.568s][info][gc,heap     ] GC(8)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[4.568s][info][gc,heap     ] GC(8)  Capacity:     4032M (99%)        4064M (100%)       4064M (100%)       4072M (100%)       4072M (100%)       4032M (99%)    
[4.568s][info][gc,heap     ] GC(8)      Free:       66M (2%)           34M (1%)           34M (1%)           34M (1%)           66M (2%)            2M (0%)     
[4.568s][info][gc,heap     ] GC(8)      Used:     4006M (98%)        4038M (99%)        4038M (99%)        4038M (99%)        4070M (100%)       4006M (98%)    
[4.568s][info][gc,heap     ] GC(8)      Live:         -              3841M (94%)        3841M (94%)        3841M (94%)            -                  -          
[4.568s][info][gc,heap     ] GC(8) Allocated:         -                32M (1%)           32M (1%)           64M (2%)             -                  -          
[4.568s][info][gc,heap     ] GC(8)   Garbage:         -               164M (4%)          164M (4%)          132M (3%)             -                  -          
[4.568s][info][gc,heap     ] GC(8) Reclaimed:         -                  -                 0M (0%)           32M (1%)             -                  -          
[4.568s][info][gc          ] GC(8) Garbage Collection (Allocation Rate) 4006M(98%)->4038M(99%)
[4.574s][info][gc,start    ] GC(9) Garbage Collection (Allocation Rate)
[4.574s][info][gc,task     ] GC(9) Using 2 workers
[4.574s][info][gc,phases   ] GC(9) Pause Mark Start 0.006ms
[4.590s][info][gc,phases   ] GC(9) Concurrent Mark 15.636ms
[4.590s][info][gc,phases   ] GC(9) Pause Mark End 0.020ms
[4.590s][info][gc,phases   ] GC(9) Concurrent Mark Free 0.001ms
[4.591s][info][gc,phases   ] GC(9) Concurrent Process Non-Strong References 0.555ms
[4.591s][info][gc,phases   ] GC(9) Concurrent Reset Relocation Set 0.001ms
[4.593s][info][gc,phases   ] GC(9) Concurrent Select Relocation Set 1.646ms
[4.593s][info][gc,phases   ] GC(9) Pause Relocate Start 0.005ms
[4.593s][info][gc,phases   ] GC(9) Concurrent Relocate 0.118ms
[4.593s][info][gc,load     ] GC(9) Load: 0.00/0.00/0.00
[4.593s][info][gc,mmu      ] GC(9) MMU: 2ms/0.0%, 5ms/3.0%, 10ms/51.5%, 20ms/75.8%, 50ms/90.3%, 100ms/95.2%
[4.593s][info][gc,marking  ] GC(9) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[4.593s][info][gc,marking  ] GC(9) Mark Stack Usage: 32M
[4.593s][info][gc,nmethod  ] GC(9) NMethods: 405 registered, 53 unregistered
[4.593s][info][gc,metaspace] GC(9) Metaspace: 1M used, 1M committed, 1088M reserved
[4.593s][info][gc,ref      ] GC(9) Soft: 116 encountered, 0 discovered, 0 enqueued
[4.593s][info][gc,ref      ] GC(9) Weak: 205 encountered, 0 discovered, 0 enqueued
[4.593s][info][gc,ref      ] GC(9) Final: 0 encountered, 0 discovered, 0 enqueued
[4.593s][info][gc,ref      ] GC(9) Phantom: 172 encountered, 137 discovered, 0 enqueued
[4.593s][info][gc,reloc    ] GC(9) Small Pages: 3 / 6M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.593s][info][gc,reloc    ] GC(9) Medium Pages: 126 / 4032M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.593s][info][gc,reloc    ] GC(9) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.593s][info][gc,reloc    ] GC(9) Forwarding Usage: 0M
[4.593s][info][gc,heap     ] GC(9) Min Capacity: 8M(0%)
[4.593s][info][gc,heap     ] GC(9) Max Capacity: 4072M(100%)
[4.593s][info][gc,heap     ] GC(9) Soft Max Capacity: 4072M(100%)
[4.593s][info][gc,heap     ] GC(9)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[4.593s][info][gc,heap     ] GC(9)  Capacity:     4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)   
[4.593s][info][gc,heap     ] GC(9)      Free:       34M (1%)            2M (0%)            2M (0%)            2M (0%)           34M (1%)            2M (0%)     
[4.593s][info][gc,heap     ] GC(9)      Used:     4038M (99%)        4070M (100%)       4070M (100%)       4070M (100%)       4070M (100%)       4038M (99%)    
[4.593s][info][gc,heap     ] GC(9)      Live:         -              3881M (95%)        3881M (95%)        3881M (95%)            -                  -          
[4.593s][info][gc,heap     ] GC(9) Allocated:         -                32M (1%)           32M (1%)           32M (1%)             -                  -          
[4.593s][info][gc,heap     ] GC(9)   Garbage:         -               156M (4%)          156M (4%)          156M (4%)             -                  -          
[4.593s][info][gc,heap     ] GC(9) Reclaimed:         -                  -                 0M (0%)            0M (0%)             -                  -          
[4.593s][info][gc          ] GC(9) Garbage Collection (Allocation Rate) 4038M(99%)->4070M(100%)
[4.594s][info][gc,start    ] GC(10) Garbage Collection (Allocation Stall)
[4.594s][info][gc,ref      ] GC(10) Clearing All SoftReferences
[4.594s][info][gc,task     ] GC(10) Using 2 workers
[4.594s][info][gc,ref      ] GC(10) Clearing All SoftReferences
[4.594s][info][gc,phases   ] GC(10) Pause Mark Start 0.033ms
[4.606s][info][gc,phases   ] GC(10) Concurrent Mark 11.974ms
[4.606s][info][gc,phases   ] GC(10) Pause Mark End 0.009ms
[4.606s][info][gc,phases   ] GC(10) Concurrent Mark Free 0.001ms
[4.621s][info][gc,phases   ] GC(10) Concurrent Process Non-Strong References 14.894ms
[4.621s][info][gc,phases   ] GC(10) Concurrent Reset Relocation Set 0.000ms
[4.622s][info][gc,phases   ] GC(10) Concurrent Select Relocation Set 1.608ms
[4.623s][info][gc,phases   ] GC(10) Pause Relocate Start 0.007ms
[4.623s][info][gc,phases   ] GC(10) Concurrent Relocate 0.033ms
[4.623s][info][gc,load     ] GC(10) Load: 0.00/0.00/0.00
[4.623s][info][gc,mmu      ] GC(10) MMU: 2ms/0.0%, 5ms/3.0%, 10ms/51.5%, 20ms/75.8%, 50ms/90.3%, 100ms/95.2%
[4.623s][info][gc,marking  ] GC(10) Mark: 2 stripe(s), 1 proactive flush(es), 1 terminate flush(es), 0 completion(s), 0 continuation(s) 
[4.623s][info][gc,marking  ] GC(10) Mark Stack Usage: 32M
[4.623s][info][gc,nmethod  ] GC(10) NMethods: 405 registered, 53 unregistered
[4.623s][info][gc,metaspace] GC(10) Metaspace: 1M used, 1M committed, 1088M reserved
[4.623s][info][gc,ref      ] GC(10) Soft: 110 encountered, 68 discovered, 40 enqueued
[4.623s][info][gc,ref      ] GC(10) Weak: 205 encountered, 80 discovered, 5 enqueued
[4.623s][info][gc,ref      ] GC(10) Final: 0 encountered, 0 discovered, 0 enqueued
[4.623s][info][gc,ref      ] GC(10) Phantom: 172 encountered, 58 discovered, 0 enqueued
[4.623s][info][gc,reloc    ] GC(10) Small Pages: 3 / 6M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.623s][info][gc,reloc    ] GC(10) Medium Pages: 127 / 4064M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.623s][info][gc,reloc    ] GC(10) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0
[4.623s][info][gc,reloc    ] GC(10) Forwarding Usage: 0M
[4.623s][info][gc,heap     ] GC(10) Min Capacity: 8M(0%)
[4.623s][info][gc,heap     ] GC(10) Max Capacity: 4072M(100%)
[4.623s][info][gc,heap     ] GC(10) Soft Max Capacity: 4072M(100%)
[4.623s][info][gc,heap     ] GC(10)                Mark Start          Mark End        Relocate Start      Relocate End           High               Low         
[4.623s][info][gc,heap     ] GC(10)  Capacity:     4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)       4072M (100%)   
[4.623s][info][gc,heap     ] GC(10)      Free:        2M (0%)            2M (0%)            2M (0%)            2M (0%)            2M (0%)            2M (0%)     
[4.623s][info][gc,heap     ] GC(10)      Used:     4070M (100%)       4070M (100%)       4070M (100%)       4070M (100%)       4070M (100%)       4070M (100%)   
[4.623s][info][gc,heap     ] GC(10)      Live:         -              3912M (96%)        3912M (96%)        3912M (96%)            -                  -          
[4.623s][info][gc,heap     ] GC(10) Allocated:         -                 0M (0%)            0M (0%)            0M (0%)             -                  -          
[4.623s][info][gc,heap     ] GC(10)   Garbage:         -               157M (4%)          157M (4%)          157M (4%)             -                  -          
[4.623s][info][gc,heap     ] GC(10) Reclaimed:         -                  -                 0M (0%)            0M (0%)             -                  -          
[4.623s][info][gc          ] GC(10) Garbage Collection (Allocation Stall) 4070M(100%)->4070M(100%)
[4.624s][info][gc          ] Allocation Stall (main) 31.819ms
[4.624s][info][gc          ] Out Of Memory (main)
[4.639s][info][gc,heap,exit] Heap
[4.639s][info][gc,heap,exit]  ZHeap           used 4072M, capacity 4072M, max capacity 4072M
[4.639s][info][gc,heap,exit]  Metaspace       used 1258K, committed 1408K, reserved 1114112K
[4.639s][info][gc,heap,exit]   class space    used 96K, committed 192K, reserved 1048576K


```

</details>

## 1、基础信息分析

<details>
    <summary>基础日志（未涉及垃圾回收）</summary>

| 基础日志分析   |                                                                                                |
|----------|------------------------------------------------------------------------------------------------|
| 相对时间     | 日志内容及解释                                                                                        |
| [0.013s] | [gc,init] Initializing The Z Garbage  Collector：  JVM在启动0.013秒时开始初始化ZGC。                       |
| [0.014s] | [gc,init] Version: 21.0.3+9-LTS (release)：  使用的是Java 21的版本21.0.3+9-LTS（长期支持版本）。                |
| [0.014s] | [gc,init] Using legacy single-generation  mode：  使用传统的单代模式，这意味着不区分年轻代和老年代。                     |
| [0.014s] | [gc,init] NUMA Support: Disabled：  非统一内存访问（NUMA）支持被禁用。                                         |
| [0.014s] | [gc,init] CPUs: 8 total, 8 available：  系统有8个CPU核心，全部可用。                                        |
| [0.014s] | [gc,init] Memory: 16281M：  系统有16281MB（约15.9GB）的内存。                                             |
| [0.014s] | [gc,init] Large Page Support: Disabled：  大页内存支持被禁用。                                            |
| [0.014s] | [gc,init] GC Workers: 2 (dynamic)：  有2个动态分配的GC工作线程。                                            |
| [0.015s] | [gc,init] Address Space Type:  Contiguous/Unrestricted/Complete：  地址空间类型为连续、无限制、完整。            |
| [0.015s] | [gc,init] Address Space Size: 65152M x 3  = 195456M：  地址空间大小为65152MB x 3 = 195456MB（约190.5GB）。 |
| [0.015s] | [gc,init] Min Capacity: 8M：  最小堆容量为8MB。                                                        |
| [0.015s] | [gc,init] Initial Capacity: 256M：  初始堆容量为256MB。                                                |
| [0.015s] | [gc,init] Max Capacity: 4072M：  最大堆容量为4072MB（约3.98GB）。                                         |
| [0.015s] | [gc,init] Medium Page Size: 32M：  中等页面大小为32MB。                                                 |
| [0.015s] | [gc,init] Pre-touch: Disabled：  预触摸内存页面的功能被禁用。                                                 |
| [0.015s] | [gc,init] Uncommit: Enabled：  允许未使用的堆内存被操作系统回收。                                                |
| [0.015s] | [gc,init] Uncommit Delay: 300s：  未使用内存被回收前的延迟时间为300秒。                                          |
| [0.027s] | [gc,init] Runtime Workers: 5：  有5个运行时工作线程。                                                     |
| [0.028s] | [gc ] Using The Z Garbage Collector：  确认使用Z垃圾收集器。                                              |

</details>

## 2、垃圾回收日志
下图是一次GC的全过程
![img.png](img.png)
<details>
    <summary>垃圾回收过程日志</summary>

| GC日志分析                                     |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
|--------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| GC启动：                                      |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.460s]                                   | [gc,start] GC(0) Garbage Collection  (Warmup)<br> 在0.460秒时，JVM开始了一次垃圾收集（GC），这是一次预热（Warmup）GC。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| GC任务<br>                                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.461s]                                   | [gc,task]  GC(0) Using 2 workers<br>使用了2个GC工作线程。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| GC阶段<br>                                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.461s]                                   | [gc,phases]  GC(0) Pause Mark Start 0.007ms<br>  标记阶段开始，暂停时间为0.007毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| [0.475s]                                   | [gc,phases]  GC(0) Concurrent Mark 14.514ms<br>  并发标记阶段，耗时14.514毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| [0.476s]                                   | [gc,phases]  GC(0) Pause Mark End 0.018ms<br>  标记阶段结束，暂停时间为0.018毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| [0.476s]                                   | [gc,phases]  GC(0) Concurrent Process Non-Strong References 0.650ms<br>  并发处理非强引用阶段，耗时0.650毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| [0.476s]                                   | [gc,phases]  GC(0) Concurrent Reset Relocation Set 0.001ms<br>  并发重置重定位集阶段，耗时0.001毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| [0.485s]                                   | [gc,phases]  GC(0) Concurrent Select Relocation Set 9.009ms<br>  并发选择重定位集阶段，耗时9.009毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| [0.486s]                                   | [gc,phases]  GC(0) Pause Relocate Start 0.013ms<br>  重定位阶段开始，暂停时间为0.013毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| [0.487s]                                   | [gc,phases]  GC(0) Concurrent Relocate 0.810ms<br>  并发重定位阶段，耗时0.810毫秒。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| GC负载<br>                                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,load]  GC(0) Load: 0.00/0.00/0.00<br>  GC负载信息。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| GC（Minimum  Mutator Utilisation）=非GC时间/时间片 |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,mmu]  GC(0) MMU: 2ms/99.1%, 5ms/99.6%, 10ms/99.8%, 20ms/99.8%, 50ms/99.9%,  100ms/100.0%<br>  是一个指标2ms/99.1%在 2 毫秒的时间片内，至少 99.1% 的时间用于应用程序线程的执行。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| GC标记                                       |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,marking]  GC(0) Mark: 2 stripe(s), 2 proactive flush(es), 1 terminate flush(es), 0  completion(s), 0 continuation(s)<br>  2 stripe(s)：表示标记阶段分为两个条带（stri），每个条带是并行标记的pe一部分。  <br>2 proactive flush(es)：表示在标记过程中进行了两次主动刷新（flush），这通常是为了确保标记信息及时更新。<br>  1 terminate flush(es)：表示在标记阶段结束时进行了1次终止刷新，确保所有标记信息都已处理完毕。<br>  0 completion(s)：表示没有完成的标记任务。 <br> 0 continuation(s)：表示没有需要继续的标记任务。                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| [0.487s]                                   | [gc,marking]  GC(0) Mark Stack Usage: 32M<br>  表示在标记阶段使用了 32MB 的标记栈（Mark Stack）。标记栈用于存储在标记过程中需要处理的对象引用。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| GC方法<br>                                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,nmethod] GC(0) NMethods: 289  registered, 41 unregistered<br>  289 registered：当前注册的本地方法（NMethods）数量为 289，这些方法已经被编译并存储在代码缓存中，供 JVM 执行。  <br>41 unregistered：当前未注册的本地方法数量为 41，这些方法可能已经被移除或不再使用。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| 元空间<br>                                    |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,metaspace]  GC(0) Metaspace: 0M used, 0M committed, 1088M reserved<br>  0M used<br>当前没有使用的元空间（Metaspace）。  0M committed<br>当前没有分配的元空间。  1088M reserved<br>预留了 1088MB 的元空间。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| 引用处理<br>                                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,ref] GC(0) Soft: 54 encountered, 0  discovered, 0 enqueued<br>[gc,ref] GC(0) Weak: 171 encountered, 48  discovered, 45 enqueued<br>[gc,ref] GC(0) Final: 0 encountered, 0  discovered, 0 enqueued<br>[gc,ref] GC(0) Phantom: 187 encountered,  90 discovered, 15 enqueued<br>软引用（Soft Reference）：<br>54 encountered：遇到了 54 个软引用。  0 discovered：发现了  0 个需要处理的软引用。  0 enqueued：入队了 0 个软引用。  <br/>弱引用（Weak Reference）：<br> 171 encountered：遇到了 171 个弱引用。  48 discovered：发现了  48 个需要处理的弱引用。  45 enqueued：入队了  45 个弱引用。  <br/>终结引用（Final Reference）：<br>  0 encountered：遇到了0  个终结引用。  0 discovered：发现了0  个需要处理的终结引用。  0 enqueued：入队了0 个终结引用。  <br/>虚引用（Phantom Reference）：<br>187 encountered：遇到了 187 个虚引用。  90 discovered：发现了  90 个需要处理的虚引用。  15 enqueued：入队了  15 个虚引用。  <br/>强引用是默认的引用类型，任何通过普通变量引用的对象都是强引用。强引用的对象在 GC  时不会被回收，除非没有任何强引用指向它们。因此，强引用不需要在 GC 日志中单独记录。 |
| 重定位<br>                                    |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [0.487s]                                   | [gc,reloc]  GC(0) Small Pages: 6 / 12M, Empty: 0M, Relocated: 0M, In-Place: 0<br>[gc,reloc]  GC(0) Medium Pages: 14 / 448M, Empty: 0M, Relocated: 0M, In-Place: 0<br>[gc,reloc]  GC(0) Large Pages: 0 / 0M, Empty: 0M, Relocated: 0M, In-Place: 0<br>[gc,reloc] GC(0) Forwarding Usage: 0M<br>Small Pages：  6 / 12M：表示有 6 个小页面，总共 12MB。<br/>Empty: 0M：没有空闲的小页面。<br/>Relocated: 0M：没有小页面被重新定位。  <br/>In-Place: 0：没有小页面在原地处理。<br/>Medium Pages:14 / 448M：表示有 14 个中等大小的页面，总共 448MB。<br/>Empty: 0M：没有空闲的中等页面。  <br/>Relocated: 0M：没有中等页面被重新定位。<br/>In-Place: 0：没有中等页面在原地处理。<br/>Large Pages: 0 / 0M：没有大页面。  <br/>Empty: 0M：没有空闲的大页面。  <br/>Relocated: 0M：没有大页面被重新定位。  <br/>In-Place: 0：没有大页面在原地处理。  <br/>Forwarding Usage：  0M：没有使用转发内存。                                                                                                                                 |
| 堆内存<br>                                    | [gc,heap     ] GC(0) Min Capacity: 8M(0%)[gc,heap     ] GC(0) Max Capacity: 4072M(100%)<br/>[gc,heap     ] GC(0) Soft Max Capacity: 4072M(100%)Min Capacity: 8M (0%)<br>最小堆容量为 8MB，占总容量的 0%。这是 JVM 启动时的初始堆大小。<br/>Max Capacity: 4072M (100%)：最大堆容量为 4072MB，占总容量的 100%。这是 JVM 可以扩展到的最大堆大小。：<br/>Soft Max Capacity: 4072M (100%)：软最大堆容量为 4072MB，占总容量的 100%。这是 JVM 在正常情况下不会超过的堆大小，但在特殊情况下可以临时超过。                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |

堆内存日志
![img_1.png](image/img_1.png)

| 时间   | 0.487s      | 0.487s      | 0.487s         | 0.487s       | 0.487s      | 0.487s      |
|------|-------------|-------------|----------------|--------------|-------------|-------------|
| 阶段   | Mark Start  | Mark End    | Relocate Start | Relocate End | High        | Low         |
| 容量变化 | 480M (12%)  | 512M (13%)  | 544M (13%)     | 544M (13%)   | 544M (13%)  | 480M (12%)  |
| 空闲变化 | 3612M (89%) | 3580M (88%) | 3546M (87%)    | 3548M (87%)  | 3612M (89%) | 3542M (87%) |
| 使用变化 | 460M (11%)  | 492M (12%)  | 526M (13%)     | 524M (13%)   | 530M (13%)  | 460M (11%)  |
| 活跃对象 | -           | 437M (11%)  | 437M (11%)     | 437M (11%)   | -           | -           |
| 分配内存 | -           | 32M (1%)    | 66M (2%)       | 67M (2%)     | -           | -           |
| 垃圾内存 | -           | 22M (1%)    | 22M (1%)       | 18M (0%)     | -           | -           |
| 回收内存 | -           | -           | 0M (0%)        | 3M (0%)      | -           | -           |
</details>


## 3、垃圾回收器特点的体现
### 3.1 垃圾回收过程中的暂停
为了能更好的对比，没有删除其他的日志

| 基础日志分析     |                                                                                                            |
|------------|------------------------------------------------------------------------------------------------------------|
| GC阶段<br>   |                                                                                                            |
| [0.461s]   |  <span style="color:red">[gc,phases] GC(0) Pause Mark Start 0.007ms</span> <br>  标记阶段开始，暂停时间为0.007毫秒。 |
| [0.475s]   | [gc,phases]  GC(0) Concurrent Mark 14.514ms<br>  并发标记阶段，耗时14.514毫秒。                                        |
| [0.476s]   |  <span style="color:red">[gc,phases]  GC(0) Pause Mark End 0.018ms<br></span>  标记阶段结束，暂停时间为0.018毫秒。                                        |
| [0.476s]   | [gc,phases]  GC(0) Concurrent Process Non-Strong References 0.650ms<br>  并发处理非强引用阶段，耗时0.650毫秒。             |
| [0.476s]   | [gc,phases]  GC(0) Concurrent Reset Relocation Set 0.001ms<br>  并发重置重定位集阶段，耗时0.001毫秒。                      |
| [0.485s]   | [gc,phases]  GC(0) Concurrent Select Relocation Set 9.009ms<br>  并发选择重定位集阶段，耗时9.009毫秒。                     |
| [0.486s]   |  <span style="color:red">[gc,phases]  GC(0) Pause Relocate Start 0.013ms</span><br>  重定位阶段开始，暂停时间为0.013毫秒。                                 |
| [0.487s]   | [gc,phases]  GC(0) Concurrent Relocate 0.810ms<br>  并发重定位阶段，耗时0.810毫秒。                                     |
其中，表里有三次进行了程序的暂停，分别是

| 阶段                               | 解释                                                                                                                                                                     |
|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Phase-Pause Mark Start（初始标记）     | 这是垃圾回收过程的开始，ZGC会扫描所有的GC Roots，并标记直接可达的对象。由于需要访问所有的GC Roots，这个阶段需要STW暂停。这个阶段通常很快，因为GC Roots的数量相对较少。<br>                                                                 |
| Phase-Pause Mark End（再次标记）       | 在并发标记阶段之后，ZGC会进行再次标记，这个阶段也是STW的。它的目的是重新标记那些在并发标记阶段可能发生变化的对象。这个阶段的暂停时间通常也很短。<br>                                                                                        |
| Phase-Pause Relocate Start（初始转移） | 在这个阶段，ZGC会开始转移阶段，这也是一个STW暂停。在这个阶段，ZGC会处理所有存活的对象，将它们复制到新的内存地址上。虽然内存分配通常耗时非常短，但对象成员变量的复制耗时可能较长，因为复制耗时与存活对象数量和对象复杂度成正比。对象越复杂，复制耗时越长。这个阶段的耗时可能会比前两个阶段长，因为它需要处理所有存活的对象。<br> |

### 3.2 垃圾回收策略

| 日志                                           | 阶段 | 解释 |
|----------------------------------------------|----|----|
| GC(0) Garbage Collection (Warmup)<br>        | 预热垃圾回收   |  在 JVM 启动时进行的第一次垃圾回收，用于初始化和预热内存管理系统  |
| GC(3) Garbage Collection (Proactive)<br>         |  主动垃圾回收  |  由 ZGC 主动触发，以确保内存使用保持在合理范围内，避免内存不足的情况  |
| GC(4) Garbage Collection (Allocation Rate) <br>  |  分配率垃圾回收  |  当对象分配速率过高时触发，以防止内存快速耗尽  |
| GC(10) Garbage Collection (Allocation Stall)<br> |  分配停滞垃圾回收  |  当内存分配出现停滞（例如没有足够的空闲内存页面）时触发，以确保内存分配能够继续进行  |

# 5、调优 、、todo
