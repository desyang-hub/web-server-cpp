## 问题


test.cc 中的速度最快，没有使用锁，之际用裸指针，通过EPOLLIN | EPOLLET | EPOLLONESHOT 来实现互斥,只有当任务完成后（建议手动EPOLL_CTL_DEL）delete，否则继续EPOLL_CTL_MOD将事件添加到监听