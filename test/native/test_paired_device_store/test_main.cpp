#include "paired_device_store.h"
#include "../../../src/paired_device_store.cpp"

#include <assert.h>
#include <string>

int main() {
    PairedDeviceStore store;

    // 1. 初始状态：count==0，isPaired 任意地址返回 false
    assert(store.count() == 0);
    assert(store.isPaired("AA:BB:CC:DD:EE:FF") == false);

    // 2. 添加一个设备：count==1，isPaired 该地址返回 true
    store.add("AA:BB:CC:DD:EE:FF");
    assert(store.count() == 1);
    assert(store.isPaired("AA:BB:CC:DD:EE:FF") == true);

    // 3. 重复添加同一地址：count 不增加
    store.add("AA:BB:CC:DD:EE:FF");
    assert(store.count() == 1);

    // 4. 添加满 3 个：count==3
    store.add("11:22:33:44:55:66");
    store.add("77:88:99:AA:BB:CC");
    assert(store.count() == 3);
    assert(store.isPaired("11:22:33:44:55:66") == true);
    assert(store.isPaired("77:88:99:AA:BB:CC") == true);

    // 5. 超过上限 FIFO 淘汰：添加第 4 个，最旧的被淘汰，count 仍为 3
    store.add("DE:AD:BE:EF:00:00");
    assert(store.count() == 3);
    assert(store.isPaired("AA:BB:CC:DD:EE:FF") == false);  // 最旧被淘汰
    assert(store.isPaired("11:22:33:44:55:66") == true);
    assert(store.isPaired("77:88:99:AA:BB:CC") == true);
    assert(store.isPaired("DE:AD:BE:EF:00:00") == true);

    // 6. 序列化/反序列化：serialize 后 deserialize 到新对象，内容一致
    const std::string serialized = store.serialize();
    PairedDeviceStore store2;
    store2.deserialize(serialized);
    assert(store2.count() == 3);
    assert(store2.isPaired("11:22:33:44:55:66") == true);
    assert(store2.isPaired("77:88:99:AA:BB:CC") == true);
    assert(store2.isPaired("DE:AD:BE:EF:00:00") == true);
    assert(store2.isPaired("AA:BB:CC:DD:EE:FF") == false);

    // 7. 反序列化空字符串：得到空列表
    PairedDeviceStore store3;
    store3.deserialize("");
    assert(store3.count() == 0);

    // 8. clear 后：count==0
    store.clear();
    assert(store.count() == 0);
    assert(store.isPaired("11:22:33:44:55:66") == false);

    return 0;
}
