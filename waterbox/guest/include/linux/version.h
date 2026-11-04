/* The guest has no Linux kernel. Boost.Asio reads this to pick epoll and the
 * like; a version of 0 picks none of them. */
#pragma once
#define LINUX_VERSION_CODE 0
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
