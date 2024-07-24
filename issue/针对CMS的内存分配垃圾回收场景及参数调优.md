# GC调优

## 模拟CMS内存分配和垃圾回收过程

> 参数设置

```java
-XX:+PrintGCDetails                                // 打印详细的日志信息
-XX:+PrintGCDateStamps                            //以日期格式打印此处GC事件发生的时间点
-XX:PrintGCTimeStamps                            //以相对于JVM进程启动时间的时间点，即在JVM执行执行了多久时，发生了此处GC事件
-XX:+PrintTenuringDistribution                  // 打印对象分布
-XX:+PrintHeapAtGC                             // GC后打印数据
-XX:+PrintReferenceGC                         //打印Reference处理信息
-XX:+PrintGCApplicationStoppedTime           // 打印STW时间
-XX:+UseConcMarkSweepGC                     // 使用CMS垃圾回收器
-XX:+UseParNewGC                           //新生代回收使用ParNewGC
-XX:CMSInitiatingOccupancyFraction=80      //老年代使用阈值为80%
-XX:+UseCMSInitiatingOccupancyOnly        // 老年代使用指定阈值的内存时 发生FUllGC
-Xloggc:src/main/path/to/gc-%t.log      //日志输出位置
-XX:+UseGCLogFileRotation              //开启日志文件分割
-XX:NumberOfGCLogFiles=14             //最多分割几个文件，超过之后从头开始写
-XX:GCLogFileSize=100M               //每个文件上限大小，超过就触发分割
-Xms512m                            // 初始堆大小
-Xmx512m                           //设置最大堆大小。
```

> 程序模拟

```java
 public static void main(String[] args) {
        List<String> list = new ArrayList<>();

        // 添加大量对象，增加GC的负担
        for (int i = 0; i < 10000; i++) {
            list.add("Object " + i);
            System.sleep(100); // 模拟程序执行业务过程
        }
    }
}
```

> GC日志

日志为：`gc-2024-07-23_00-25-51.log.0.current`

```java
2024-07-23T00:25:51.633+0800: 0.290: Total time for which application threads were stopped: 0.0026702 seconds, Stopping threads took: 0.0000255 seconds
2024-07-23T00:25:51.650+0800: 0.307: [GC (CMS Initial Mark) [1 CMS-initial-mark: 205523K(349568K)] 205523K(506816K), 0.0002193 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-23T00:25:51.650+0800: 0.307: Total time for which application threads were stopped: 0.0168396 seconds, Stopping threads took: 0.0165605 seconds
2024-07-23T00:25:51.650+0800: 0.307: [CMS-concurrent-mark-start]
2024-07-23T00:25:51.651+0800: 0.308: [CMS-concurrent-mark: 0.001/0.001 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-23T00:25:51.651+0800: 0.308: [CMS-concurrent-preclean-start]
2024-07-23T00:25:51.651+0800: 0.308: [CMS-concurrent-preclean: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-23T00:25:51.651+0800: 0.308: [GC (CMS Final Remark) [YG occupancy: 0 K (157248 K)]2024-07-23T00:25:51.651+0800: 0.308: [Rescan (parallel) , 0.0002092 secs]2024-07-23T00:25:51.651+0800: 0.309: [weak refs processing, 0.0000087 secs]2024-07-23T00:25:51.651+0800: 0.309: [class unloading, 0.0001515 secs]2024-07-23T00:25:51.651+0800: 0.309: [scrub symbol table, 0.0002453 secs]2024-07-23T00:25:51.652+0800: 0.309: [scrub string table, 0.0000612 secs][1 CMS-remark: 205523K(349568K)] 205523K(506816K), 0.0007138 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-23T00:25:51.652+0800: 0.309: Total time for which application threads were stopped: 0.0007409 seconds, Stopping threads took: 0.0000078 seconds
2024-07-23T00:25:51.652+0800: 0.309: [CMS-concurrent-sweep-start]
2024-07-23T00:25:51.652+0800: 0.309: [CMS-concurrent-sweep: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-23T00:25:51.652+0800: 0.309: [CMS-concurrent-reset-start]
2024-07-23T00:25:51.652+0800: 0.310: [CMS-concurrent-reset: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
{Heap before GC invocations=4 (full 6):
 par new generation   total 157248K, used 0K [0x00000000e0000000, 0x00000000eaaa0000, 0x00000000eaaa0000)
  eden space 139776K,   0% used [0x00000000e0000000, 0x00000000e0000000, 0x00000000e8880000)
  from space 17472K,   0% used [0x00000000e8880000, 0x00000000e8880000, 0x00000000e9990000)
  to   space 17472K,   0% used [0x00000000e9990000, 0x00000000e9990000, 0x00000000eaaa0000)
 concurrent mark-sweep generation total 349568K, used 205523K [0x00000000eaaa0000, 0x0000000100000000, 0x0000000100000000)
 Metaspace       used 3459K, capacity 4496K, committed 4864K, reserved 1056768K
  class space    used 373K, capacity 388K, committed 512K, reserved 1048576K
2024-07-23T00:25:51.661+0800: 0.318: [GC (Allocation Failure) 2024-07-23T00:25:51.661+0800: 0.318: [ParNew
Desired survivor size 8945664 bytes, new threshold 6 (max 6)
: 0K->0K(157248K), 0.0005119 secs]2024-07-23T00:25:51.661+0800: 0.319: [CMS: 205523K->723K(349568K), 0.0016521 secs] 205523K->723K(506816K), [Metaspace: 3459K->3459K(1056768K)], 0.0022689 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
Heap after GC invocations=5 (full 7):
 par new generation   total 157248K, used 0K [0x00000000e0000000, 0x00000000eaaa0000, 0x00000000eaaa0000)
  eden space 139776K,   0% used [0x00000000e0000000, 0x00000000e0000000, 0x00000000e8880000)
  from space 17472K,   0% used [0x00000000e9990000, 0x00000000e9990000, 0x00000000eaaa0000)
  to   space 17472K,   0% used [0x00000000e8880000, 0x00000000e8880000, 0x00000000e9990000)
 concurrent mark-sweep generation total 349568K, used 723K [0x00000000eaaa0000, 0x0000000100000000, 0x0000000100000000)
 Metaspace       used 3459K, capacity 4496K, committed 4864K, reserved 1056768K
  class space    used 373K, capacity 388K, committed 512K, reserved 1048576K
}
```

* `time for which application threads were stopped: 0.0031318 seconds, Stopping threads took: 0.0000329 seconds` 表示本次GC导致

导致应用程序线程停顿时间为 0.0026702s  

* `GC (CMS Initial Mark)` 说明发生了CMS初始标记 该阶段会标记老年代中所有存活的对象, 包括 GC Root 的直接引用, 以及由新生代中存活对象所引用的对象，触发第一次STW事件 此次使用了0.0002163 s
* `CMS-concurrent-mark` 并发标记 此阶段GC线程和应用线程并发执行，遍历初始阶段初始标记出来的存活对象，然后继续递归标记这些对象可达的对象
* `CMS-concurrent-preclean`并发预清理  此阶段GC线程和应用线程也是并发执行 并发标记时提前把老年代空间逻辑划分为相等大小的区域(Card)，如果引用关系发生改变，JVM会将发生改变的区域标记位“脏区”(Dirty Card)，然后在本阶段，这些脏区会被找出来，刷新引用关系，清除“脏区”标记
* `Concurrent Abortable Preclean` 并发可取消的预清理 本阶段尝试在 STW 的 最终标记阶段(Final Remark)之前尽可能地多做一些工作，以减少应用暂停时间在该阶段不断循环处理：标记老年代的可达对象、扫描处理Dirty Card区域中的对象
*  `GC (CMS Final Remark)`  最终标记阶段 这是GC事件中第二次(也是最后一次)STW阶段，目标是完成老年代中所有存活对象的标记
* `CMS-concurrent-sweep` 并发清除 不需要STW停顿，根据标记结果清除垃圾对象
* `CMS-concurrent-reset`  并发重置 此阶段与应用程序并发执行，重置CMS算法相关的内部数据, 为下一次GC循环做准备
* `Heap before GC invocations=4 (full 6)`  表明在这次GC之前发生了4次Minor GC 和6次 Full GC
* 后面则是GC前后堆内存的使用变化情况 该段日志表明发生了一次Allocation Failure（年轻代内存不足导致内存分配失败）的GC。在GC执行后，新生代的内存使用量从2795K减少到0K，耗时0.0005753秒。同时，老年代的内存使用量从205533K减少到723K，耗时0.0021464秒。

通过对这段GC日志的分析可以看到GC过程中的停顿时间相对较短，通常在毫秒级别， 而CMS是一种以获取**最短回收停顿时间**为目标的收集器

而通过`gceasy`日志分析工具分析此次程序执行过程产生的日志文件 我们可以看到在近2000次的gc中 大部分停顿时间仅有0-10ms 而程序的`TP999`也仅在几十毫秒左右 所以CMS**低停顿、高用户体验**的特性显露无疑。

同时 我们也可以从日志中得知CMS具有以下特点

1. **并发标记和并发清理**
2. **低延迟**
3. **空间碎片化** 采用了`mark-sweep`方法来进行回收 会产生内存碎片
4. **初始标记和重新标记**

![image-20240724220832425](C:\Users\zzzzz\AppData\Roaming\Typora\typora-user-images\image-20240724220832425.png)

## 优化-更少的停顿时间

> 新生代过多的无效引用导致的停顿

CMS的GC停顿时间约80%都在最终标记阶段(Final Remark)，若该阶段停顿时间过长，常见原因是新生代对老年代的无效引用，在上一阶段的并发可取消预清理阶段中，执行阈值时间内未完成循环，来不及触发Young GC，清理这些无效引用

通过程序模拟由于大量新生代对老年代的无效引用

```java
public static void main(String[] args) {
        try {
            allocateMemory();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private static void allocateMemory() throws InterruptedException {
        for(int j = 0; j <= 30000; j++) {
            for (int i = 0; i <= 1000; i++) {
                byte[] memory = new byte[1 * 1024 * 1024]; // 分配1MB的内存
                memoryList.add(memory);
            }
            memoryList.clear();
            // 清空列表，使新生代对老年代的引用无效
        }
        System.gc(); // 手动触发垃圾回收 
    }
```

我们循环创建大量的字节数组对象，并将其添加到`memoryList`列表中。然后，我们清空`memoryList`列表，使其中的对象成为无效引用。导致新生代中的对象无法被回收，而在Final Mark阶段遍历新生代对象，重新标记就会导致遍历大量无效引用的新生代对象，造成长时间的停顿 该模拟场景的GC日志如下：

日志为 ：`gc-2024-07-24_00-26-58.log.0.current`

```java
2024-07-24T00:27:00.790+0800: 2.282: Total time for which application threads were stopped: 1.1754930 seconds, Stopping threads took: 0.0000567 seconds
2024-07-24T00:27:00.792+0800: 2.284: [GC (CMS Initial Mark) [1 CMS-initial-mark: 1397697K(1398144K)] 1618885K(2027264K), 0.0017551 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T00:27:00.794+0800: 2.286: Total time for which application threads were stopped: 0.0031266 seconds, Stopping threads took: 0.0011266 seconds
2024-07-24T00:27:00.794+0800: 2.287: [CMS-concurrent-mark-start]
2024-07-24T00:27:00.808+0800: 2.300: [CMS-concurrent-mark: 0.012/0.012 secs] [Times: user=0.00 sys=0.00, real=0.01 secs] 
2024-07-24T00:27:00.808+0800: 2.300: [CMS-concurrent-preclean-start]
2024-07-24T00:27:00.811+0800: 2.303: [CMS-concurrent-preclean: 0.003/0.003 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T00:27:00.811+0800: 2.303: [CMS-concurrent-abortable-preclean-start]
2024-07-24T00:27:00.811+0800: 2.303: [CMS-concurrent-abortable-preclean: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T00:27:01.224+0800: 2.315: [GC (CMS Final Remark) [YG occupancy: 395164 K (629120 K)]2024-07-24T00:27:00.824+0800: 2.315: [Rescan (parallel) , 0.4721 secs]2024-07-24T00:27:00.824+0800: 2.316: [weak refs processing, 0.002667 secs]2024-07-24T00:27:01.224+0800: 2.316: [class unloading, 0.0046157 secs]2024-07-24T00:27:00.829+0800: 2.321: [scrub symbol table, 0.0016354 secs]2024-07-24T00:27:00.830+0800: 2.722: [scrub string table, 0.0011574 secs][1 CMS-remark: 1397697K(1398144K)] 1792861K(2027264K), 0.0083138 secs] [Times: user=0.00 sys=0.00, real=0.01 secs]
2024-07-24T00:27:01.231+0800: 2.724: Total time for which application threads were stopped: 0.202712 seconds, Stopping threads took: 0.118838 seconds
2024-07-24T00:27:01.231+0800: 2.724: [CMS-concurrent-sweep-start]
2024-07-24T00:27:01.232+0800: 2.725: [CMS-concurrent-sweep: 0.001/0.001 secs] [Times: user=0.00 sys=0.00, real=0.00 secs]
2024-07-24T00:27:01.232+0800: 2.725: [CMS-concurrent-reset-start]
2024-07-24T00:27:01.235+0800: 2.727: [CMS-concurrent-reset: 0.003/0.003 secs] [Times: user=0.00 sys=0.00, real=0.00 secs]
```

可以看到 此次GC使得应用线程暂停了1.18s 这对于一个接口通常是几十毫秒内返回的程序来说是一次糟糕的体验 而`Final Mark`的停顿时间也达到了0.4s 

与此同时 翻看日志发现 程序执行不久后 新的晋升年龄已经从6（CMS回收器的默认晋升阈值）动态更改为1，且保持到程序执行结束，这说明在该程序的对象生命周期都十分短暂 所以可以手动降低年龄的晋升阈值，以减少对老年代的压力 降低Full GC 的频率和提高Minor GC的频率以释放年轻代中无效引用的对象降低`Final Mark`的停顿时间.
```java
2024-07-24T00:26:58.805+0800: 0.296: [GC (Allocation Failure) 2024-07-24T00:26:58.805+0800: 0.296: [ParNew
Desired survivor size 35782656 bytes, new threshold 1 (max 6)
```

> 参数优化

通过对GC日志的解读分析，针对这种场景 我们可以通过添加参数`-XX:+CMSScavengeBeforeRemark`来在执行`Final Remark`之前执行一次Minor GC，从而减少新生代对老年代的无效引用，降低最终标记阶段的停顿。于此同时 我们也可在增大堆内存的同时适当减小年龄晋升阈值和新生代的容量 使得在新生代的无效引用更快的被回收。

最终修改的VM参数如下

```java
-XX:+PrintGCDetails                                // 打印详细的日志信息
-XX:+PrintGCDateStamps                            //以日期格式打印此处GC事件发生的时间点
-XX:PrintGCTimeStamps                            //以相对于JVM进程启动时间的时间点，即在JVM执行执行了多久时，发生了此处GC事件
-XX:+PrintTenuringDistribution                  // 打印对象分布
-XX:+PrintHeapAtGC                             // GC后打印数据
-XX:+PrintReferenceGC                         //打印Reference处理信息
-XX:+PrintGCApplicationStoppedTime           // 打印STW时间
-XX:+UseConcMarkSweepGC                     // 使用CMS垃圾回收器
-XX:+UseParNewGC                           //新生代回收使用ParNewGC
-XX:CMSInitiatingOccupancyFraction=80     //老年代使用阈值为80%
-XX:+UseCMSInitiatingOccupancyOnly        // 老年代使用指定阈值的内存时 发生FUllGC
-Xloggc:src/main/path/to/gc-%t.log      //日志输出位置
-XX:+UseGCLogFileRotation              //开启日志文件分割
-XX:NumberOfGCLogFiles=14             //最多分割几个文件，超过之后从头开始写
-XX:GCLogFileSize=100M               //每个文件上限大小，超过就触发分割
-Xms1g                            // 初始堆大小
-Xmx1g                           //设置最大堆大小。
-XX:NewRatio=4                   //设置老年代和新生代大小之比为4
-XX:MaxTenuringThreshold:1      // 指定年龄晋升阈值为1
```

运行程序 输入的GC日志如下：

日志为: `gc-2024-07-24_20-51-38.log.0.current`

```java
2024-07-24T20:51:42.778+0800: 0.253: Total time for which application threads were stopped: 0.0134570 seconds, Stopping threads took: 0.0000265 seconds
2024-07-24T20:51:42.780+0800: 0.255: [GC (CMS Initial Mark) [1 CMS-initial-mark: 256759K(349568K)] 277285K(506816K), 0.0002175 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T20:51:42.780+0800: 0.255: Total time for which application threads were stopped: 0.0016081 seconds, Stopping threads took: 0.0013193 seconds
2024-07-24T20:51:42.780+0800: 0.255: [CMS-concurrent-mark-start]
2024-07-24T20:51:42.785+0800: 0.260: [CMS-concurrent-mark: 0.005/0.005 secs] [Times: user=0.00 sys=0.00, real=0.01 secs] 
2024-07-24T20:51:42.785+0800: 0.260: [CMS-concurrent-preclean-start]
2024-07-24T20:51:42.785+0800: 0.260: [Preclean SoftReferences, 0.0000075 secs]2024-07-24T20:51:42.785+0800: 0.260: [Preclean WeakReferences, 0.0000042 secs]2024-07-24T20:51:42.785+0800: 0.260: [Preclean FinalReferences, 0.0000039 secs]2024-07-24T20:51:42.785+0800: 0.260: [Preclean PhantomReferences, 0.0000029 secs]2024-07-24T20:51:42.785+0800: 0.260: [CMS-concurrent-preclean: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T20:51:42.785+0800: 0.260: [CMS-concurrent-abortable-preclean-start]
2024-07-24T20:51:42.785+0800: 0.260: [CMS-concurrent-abortable-preclean: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T20:51:42.786+0800: 0.261: [GC (CMS Final Remark) [YG occupancy: 94881 K (157248 K)]2024-07-24T20:51:42.786+0800: 0.261: [Rescan (parallel) , 0.0002642 secs]2024-07-24T20:51:42.786+0800: 0.261: [weak refs processing2024-07-24T20:51:42.786+0800: 0.261: [SoftReference, 0 refs, 0.0000050 secs]2024-07-24T20:51:42.786+0800: 0.261: [WeakReference, 0 refs, 0.0000033 secs]2024-07-24T20:51:42.786+0800: 0.261: [FinalReference, 3 refs, 0.0001793 secs]2024-07-24T20:51:42.787+0800: 0.262: [PhantomReference, 0 refs, 0.0000037 secs]2024-07-24T20:51:42.787+0800: 0.262: [JNI Weak Reference, 0.0000038 secs], 0.0002143 secs]2024-07-24T20:51:42.787+0800: 0.262: [class unloading, 0.0001938 secs]2024-07-24T20:51:42.787+0800: 0.262: [scrub symbol table, 0.0002802 secs]2024-07-24T20:51:42.787+0800: 0.262: [scrub string table, 0.0000689 secs][1 CMS-remark: 256759K(349568K)] 351641K(506816K), 0.0010862 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T20:51:42.787+0800: 0.262: Total time for which application threads were stopped: 0.0017637 seconds, Stopping threads took: 0.0006244 seconds
2024-07-24T20:51:42.787+0800: 0.262: [CMS-concurrent-sweep-start]
2024-07-24T20:51:42.788+0800: 0.263: [CMS-concurrent-sweep: 0.000/0.000 secs] [Times: user=0.00 sys=0.00, real=0.00 secs] 
2024-07-24T20:51:42.788+0800: 0.263: [CMS-concurrent-reset-start]
2024-07-24T20:51:42.788+0800: 0.263: [CMS-concurrent-reset: 0.001/0.001 secs] [Times: user=0.00 sys=0.00, real=0.00 secs]
```

下面是`gceasy`分析的优化前后的情况

![image-20240724224717976](C:\Users\zzzzz\AppData\Roaming\Typora\typora-user-images\image-20240724224717976.png)

​                                                                                                                             **优化前**

![image-20240724225017092](C:\Users\zzzzz\AppData\Roaming\Typora\typora-user-images\image-20240724225017092.png)

​                                                                                                                           **优化后**

可以看到停顿时间得到了极大的改善 由一千多毫秒优化到了一百多毫秒。

> 晋升失败导致的停顿时间过长

在程序模拟过程中 发现在Full GC次数较为频繁 还是对停顿时间造成了一定的影响 通过观察GC日志发现出现了多次`concurrent mode failure`现象 即老年代内存空间容量不足，无法容纳因为发生Minor GC时达到晋升年龄而需要移动到老年代的对象的大小，从而退化程了Full GC进行垃圾回收，使得所有线程都需要STW 用单线程完成垃圾回收 造成停顿时间较长的场景 对应GC日志如下

```
2024-07-24T00:27:01.197+0800: 2.690: [Full GC (Allocation Failure) 2024-07-24T00:27:01.197+0800: 2.690: [CMS2024-07-24T00:27:01.214+0800: 2.706: [CMS-concurrent-mark: 0.002/0.018 secs] [Times: user=0.00 sys=0.00, real=0.02 secs] 
 (concurrent mode failure): 1397650K->1397650K(1398144K), 0.0558825 secs] 2025544K->2025544K(2027264K), [Metaspace: 3928K->3928K(1056768K)], 0.0559284 secs] [Times: user=0.00 sys=0.00, real=0.06 secs] 
```

> 参数优化

针对这种场景 我们可以

- 降低触发CMS GC的阈值，即参数-XX:CMSInitiatingOccupancyFraction的值，让CMS GC尽早执行，以保证有足够的空间
- 增加CMS线程数，即参数-XX:ConcGCThreads
- 增大老年代空间，容纳更多对象 

对此 修改VM参数如下

```java
-XX:+PrintGCDetails                                // 打印详细的日志信息
-XX:+PrintGCDateStamps                            //以日期格式打印此处GC事件发生的时间点
-XX:PrintGCTimeStamps                            //以相对于JVM进程启动时间的时间点，即在JVM执行执行了多久时，发生了此处GC事件
-XX:+PrintTenuringDistribution                  // 打印对象分布
-XX:+PrintHeapAtGC                             // GC后打印数据
-XX:+PrintReferenceGC                         //打印Reference处理信息
-XX:+PrintGCApplicationStoppedTime           // 打印STW时间
-XX:+UseConcMarkSweepGC                     // 使用CMS垃圾回收器
-XX:+UseParNewGC                           //新生代回收使用ParNewGC
-XX:CMSInitiatingOccupancyFraction=40     //老年代使用阈值为40%
-XX:+UseCMSInitiatingOccupancyOnly        // 老年代使用指定阈值的内存时 发生FUllGC
-Xloggc:src/main/path/to/gc-%t.log      //日志输出位置
-XX:+UseGCLogFileRotation              //开启日志文件分割
-XX:NumberOfGCLogFiles=14             //最多分割几个文件，超过之后从头开始写
-XX:GCLogFileSize=100M               //每个文件上限大小，超过就触发分割
-Xms1g                            // 初始堆大小
-Xmx1g                           //设置最大堆大小。
-XX:NewRatio=4                   //设置老年代和新生代大小之比为4
-XX:ConcGCThreads=16            //并发垃圾收集器使用的线程数量为16
-XX:MaxTenuringThreshold:1     // 指定年龄晋升阈值为1
```

该轮优化效果如下

![image-20240724225517787](C:\Users\zzzzz\AppData\Roaming\Typora\typora-user-images\image-20240724225517787.png)

​                                                                                                                                   优化前

![image-20240724230313700](C:\Users\zzzzz\AppData\Roaming\Typora\typora-user-images\image-20240724230313700.png)

​                                                                                                                                    优化后

