#pragma once

class PollCallback {
public:
    virtual ~PollCallback() = default;
    PollCallback(const PollCallback&) = delete;
    PollCallback& operator=(const PollCallback&) = delete;
    PollCallback(PollCallback&&) = delete;
    PollCallback& operator=(PollCallback&&) = delete;
protected:
    PollCallback() = default;
public:
    virtual bool onPoll() = 0;
};
