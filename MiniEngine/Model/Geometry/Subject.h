#pragma once
#include "Observer.h"
#include <vector>
template<typename Entity, typename Event>
class Subject
{
private:
	std::vector<Observer<typename Entity, typename Event>*> observers_;

public:

	Subject();
	Subject<Entity, Event>(const Subject<Entity, Event>& other);
	Subject<Entity, Event>& operator=(const Subject<Entity, Event>& other)
	{
		return *this;
	}
	void addObserver(Observer<typename Entity, typename Event>* observer);
	void removeObserver(Observer<typename Entity, typename Event>* observer);

protected:
	void notify(const Entity* entity, Event event) const;
};

template<typename Entity, typename Event>
Subject<Entity, Event>::Subject()
{

}

template<typename Entity, typename Event>
void Subject<Entity, Event>::addObserver(Observer<typename Entity, typename Event>* observer)
{
	observers_.push_back(observer);
}

template<typename Entity, typename Event>
void Subject<Entity, Event>::removeObserver(Observer<typename Entity, typename Event>* observer)
{
	//observers_.era(observer);
}

template<typename Entity, typename Event>
void Subject<Entity, Event>::notify(const Entity* entity, Event event) const
{
	for (auto ob : observers_)
	{
		ob->onNotify(entity, event);
	}
}

template<typename Entity, typename Event>
Subject<Entity, Event>::Subject(const Subject<Entity, Event>& other)
{

}


