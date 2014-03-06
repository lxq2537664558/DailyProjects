#include "pch.h"

#include <sys/select.h>
#include <unistd.h>

#include "Utils.h"
#include "Poller.h"

SelectPoller::SelectPoller() {
    FD_ZERO(mFdSets + 0);
    FD_ZERO(mFdSets + 1);
    FD_ZERO(mFdSets + 2);
}
SelectPoller::~SelectPoller() {
    ASSERT(size() == 0);
}
void SelectPoller::add(int fd, void *ud, int ef) {
    ASSERT(fd < FD_SETSIZE)(fd);

    if (ef & EF_Readable) FD_SET(fd, mFdSets + 0);
    if (ef & EF_Writeable) FD_SET(fd, mFdSets + 1);
    mFd2Ud[fd] = ud;
}
void SelectPoller::update(int fd, void *ud, int ef) {
    del(fd);
    add(fd, ud, ef);
}
void SelectPoller::del(int fd) {
    ASSERT(fd < FD_SETSIZE)(fd);

    FD_CLR(fd, mFdSets + 0);
    FD_CLR(fd, mFdSets + 1);
    mFd2Ud.erase(fd);
}
bool SelectPoller::wait(vector<Event> &events, int timeout) {
    ASSERT(size() > 0);
    ASSERT(events.empty());

    fd_set tmpFdSets[3] = { mFdSets[0], mFdSets[1], mFdSets[2]};
    timeval tval = { timeout / 1000, (timeout % 1000) * 1000 };

    int n = ::select(mFd2Ud.rbegin()->first + 1, mFdSets + 0, mFdSets + 1, nullptr, &tval);
    P_ENSURE(n >= 0);
    if (n == 0) return false;

    for (auto &fdUd : mFd2Ud) {
        Event event = {0};
        if (FD_ISSET(fdUd.first, tmpFdSets + 0)) event.flag |= EF_Readable;
        if (FD_ISSET(fdUd.first, tmpFdSets + 1)) event.flag |= EF_Writeable;
        if (event.flag != 0) {
            event.ud = fdUd.second;
            events.push_back(event);
        }
    }

    return true;
}
int SelectPoller::size() {
    return mFd2Ud.size();
}

//////////////////////////////
PollPoller::PollPoller() {
}
PollPoller::~PollPoller() {
    ASSERT(size() == 0);
}
int PollPoller::size() {
    return mFds.size();
}
void PollPoller::add(int fd, void *ud, int ef) {
    pollfd pf = {fd};
    if (ef & EF_Readable) pf.events |= POLLIN;
    if (ef & EF_Writeable) pf.events |= POLLOUT;
    mFds.push_back(pf);
    mFd2Ud[fd] = ud;
}
void PollPoller::update(int fd, void *ud, int ef) {
    del(fd);
    add(fd, ud, ef);
}
void PollPoller::del(int fd) {
    mFds.erase(find_if(mFds.begin(), mFds.end(), [fd](const pollfd& pf){ return pf.fd == fd; }));
    mFd2Ud.erase(fd);
}
bool PollPoller::wait(vector<Event> &events, int timeout) {
    ASSERT(size() > 0);
    ASSERT(events.empty());

    int n = ::poll(&mFds[0], mFds.size(), timeout);
    P_ENSURE(n >= 0);
    if (n == 0) return false;

    for (pollfd &pf : mFds) {
        Event event = {0};
        if (pf.revents & POLLIN) event.flag |= EF_Readable;
        if (pf.revents & POLLOUT) event.flag |= EF_Writeable;
        if (pf.revents & (POLLERR | POLLHUP | POLLNVAL | POLLRDHUP))  event.flag |= EF_ErrFound;
        if (event.flag != 0) {
            event.ud = mFd2Ud[pf.fd];
            events.push_back(event);
        }
    }

    ASSERT((int)events.size() == n);
    return true;
}

//////////////////////////////
EPollPoller::EPollPoller(bool edgeTrigger): mSize(0), mEdgeTrigger(edgeTrigger) {
    mEp = ::epoll_create(1);
    P_ENSURE(mEp != -1);
}
EPollPoller::~EPollPoller() {
    ASSERT(size() == 0);
    ::close(mEp);
}
int EPollPoller::size() {
    return mSize;
}
void EPollPoller::add(int fd, void *ud, int ef) {
    ++mSize;
    epControl(EPOLL_CTL_ADD, fd, ud, ef);
}
void EPollPoller::update(int fd, void *ud, int ef) {
    epControl(EPOLL_CTL_MOD, fd, ud, ef);
}
void EPollPoller::del(int fd) {
    --mSize;
    epControl(EPOLL_CTL_DEL, fd, nullptr, 0);
}
void EPollPoller::epControl(int op, int fd, void *ud, int ef) {
    epoll_event event = {0};
    event.data.ptr = ud;
    if (ef & EF_Readable) event.events |= EPOLLIN;
    if (ef & EF_Writeable) event.events |= EPOLLOUT;
    if (mEdgeTrigger) event.events |= EPOLLET;
    P_ENSURE(::epoll_ctl(mEp, op, fd, &event) == 0);
}
bool EPollPoller::wait(vector<Event> &events, int timeout) {
    ASSERT(size() > 0);
    ASSERT(events.empty());

    mTmpEvents.resize(128);

    int n = ::epoll_wait(mEp, &mTmpEvents[0], mTmpEvents.size(), timeout);
    P_ENSURE(n >= 0);
    if (n == 0) return false;

    for (int i = 0; i < n; ++i) {
        epoll_event *epevent = &mTmpEvents[i];
        Event event = {epevent->data.ptr};
        if (epevent->events & EPOLLIN) event.flag |= EF_Readable;
        if (epevent->events & EPOLLOUT) event.flag |= EF_Writeable;
        if (epevent->events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) event.flag |= EF_ErrFound;
        ASSERT(event.flag != 0);
        events.push_back(event);
    }

    return true;
}