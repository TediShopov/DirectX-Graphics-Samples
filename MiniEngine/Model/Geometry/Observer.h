#pragma once
template<typename Entity, typename Event>
class Observer
{
public:
	virtual ~Observer() {}
	virtual void onNotify(const Entity* entity, Event event) = 0;
};
