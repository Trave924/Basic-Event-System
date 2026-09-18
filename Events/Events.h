#pragma once
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>


using Event = std::function<void()>;

enum EventType {
	ET_Trigger,
	ET_OneTime,
	ET_Continuous,
	ET_OnChange,
};

struct Trigger {
	virtual ~Trigger() = default;
	virtual bool IsTriggered() = 0;
};

template<typename T>
class BoolTrigger : public Trigger {
	T& value;

public:
	BoolTrigger(T& value) : value(value) {}

	bool IsTriggered() override {
		return value;
	}
};

template<typename T>
T& ReadValue(T& value) {
	return value;
}

template<typename T>
T ReadValue(std::atomic<T>& value) {
	return value.load();
}

template<typename T>
using TriggerValue = decltype(ReadValue(std::declval<T&>()));


template<typename T>
class ValueTrigger : public Trigger {
	T& value;
	std::function<bool(TriggerValue<T>)> condition;
public:
	ValueTrigger(T& value, std::function<bool(TriggerValue<T>)> condition) : value(value), condition(std::move(condition)) {}

	bool IsTriggered() override {
		return condition(ReadValue(value));
	}
};

template<typename T>
class ChangeTrigger : public Trigger {
	T& value;
	T previousValue;

public:
	ChangeTrigger(T& value): value(value), previousValue(value) {}

	bool IsTriggered() override {
		if (value != previousValue) {
			previousValue = value;
			return true;
		}

		return false;
	}
};

struct EventState {
	bool wasTriggered = false;
	bool initialized = false;
};

struct EventData {
	std::string name;
	EventType type;
	std::shared_ptr<Trigger> trigger;
	Event callback;
	Event elseCallback = nullptr;
};

template<typename T>
inline std::shared_ptr<ValueTrigger<T>> When(T& trigger, std::function<bool(TriggerValue<T>)> condition) {
	return std::make_shared<ValueTrigger<T>>(trigger, condition);
}
inline std::shared_ptr<BoolTrigger<bool>> When(bool& trigger) {
	return std::make_shared<BoolTrigger<bool>>(trigger);
}
inline std::shared_ptr<BoolTrigger<std::atomic<bool>>> When(std::atomic<bool>& trigger) {
	return std::make_shared<BoolTrigger<std::atomic<bool>>>(trigger);
}

template<typename T>
inline std::shared_ptr<ChangeTrigger<T>> Change(T& value) {
	return std::make_shared<ChangeTrigger<T>>(value);
}

template<typename T>
inline Event Set(T& value, T newValue) {
	return [&value, newValue]() {value = newValue; };
}

template<typename T>
inline Event Toggle(T& value) {
	return [&value]() {value = !value;};
}

template<typename...Args>
inline Event Do(Args&&... args) {
	return [args...]() {(args(), ...); };
}

template<typename...Args>
inline Event Print(Args&&... args) {
	return [&args...]() {(std::cout << ... << args) << std::endl; };
}

template<typename T>
Event Increment(T& value) {
	return [&value]() {value++;};
}

namespace ET {
	template<typename T>
	std::function<bool(T)> GreaterThan(T triggerer) {
		return [triggerer](T value) {return value > triggerer; };
	}
	template<typename T>
	std::function<bool(T)> LessThan(T triggerer) {
		return [triggerer](T value) {return value < triggerer; };
	}
	template<typename T>
	std::function<bool(T)> EqualTo(T triggerer) {
		return [triggerer](T value) {return value == triggerer; };
	}
	template<typename T>
	std::function<bool(T)> NotEqualTo(T triggerer) {
		return [triggerer](T value) {return value != triggerer; };
	}
	template<typename T>
	std::function<bool(T)> GreaterOrEqualThan(T triggerer) {
		return [triggerer](T value) {return value >= triggerer; };
	}
	template<typename T>
	std::function<bool(T)> LessOrEqualThan(T triggerer) {
		return [triggerer](T value) {return value <= triggerer; };
	}
}


template<typename T>
class Snapshot {
	std::atomic<int> version = 0;
	std::atomic<std::shared_ptr<T>> events;
public:
	Snapshot() {
		events.store(std::make_shared<T>());
	}

	template< typename... Args>
	void Insert(Args&&... args) {
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);
		newData->insert_or_assign(std::forward<Args>(args)...);
		events.store(newData);
		version.fetch_add(1);
	}

	void Publish(const T& data)
	{
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);

		for (const auto& item : data)
			newData->insert_or_assign(item.first, item.second);

		events.store(newData);
		version.fetch_add(1);
	}

	template<typename... Args>
	void Erase(Args&&... args) {
		auto old = events.load();
		auto newData = std::make_shared<T>(*old);

		newData->erase(std::forward<Args>(args)...);

		events.store(newData);
		version.fetch_add(1);
	}

	int Version() const { return version.load(); }

	std::shared_ptr<T> Get() {
		return events.load();
	}


};

class EventHandler {
	Snapshot<std::unordered_map<std::string, EventData>> eventsSnapshot;
	std::thread eventsThread;
	std::unordered_map<std::string, EventData> events;
	int dataVersion = 0;
	int jobVersion = 0;
	std::queue<Event> eventsJob;
	std::mutex jobsMutex;
	std::unordered_map<std::string, EventState> eventStates;
	bool batchBegined = false;
	std::unordered_map<std::string, EventData> batch;
public:
	std::atomic<bool> isEventsRunning = true;

	EventHandler() {
		eventsThread = std::thread([&]() {
			while (isEventsRunning.load()) {
				int currentVersion = eventsSnapshot.Version();
				if (dataVersion != currentVersion) {
					events = *(eventsSnapshot.Get());
					dataVersion = currentVersion;
				}

				for (auto& event : events) {
					switch (event.second.type)
					{
					case ET_Trigger:
					{
						bool triggered = event.second.trigger->IsTriggered();

						if (triggered && !eventStates[event.first].wasTriggered) {
							{
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.callback);
							}
						}
						else if (!triggered && eventStates[event.first].wasTriggered) {
							if(event.second.elseCallback){
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.elseCallback);
							}
						}

						eventStates[event.first].wasTriggered = triggered;
					}
					break;
					case ET_OneTime:
						if ((*event.second.trigger).IsTriggered()) {
							{
								std::lock_guard lock(jobsMutex);
								eventsJob.push(event.second.callback);
							}
							eventsSnapshot.Erase(event.first);
						}
						break;

					case ET_Continuous:
					{
						if (event.second.trigger->IsTriggered()) {
							std::lock_guard lock(jobsMutex);
							eventsJob.push(event.second.callback);
						}
					}
					break;
					case ET_OnChange:
					{
						if (event.second.trigger->IsTriggered()) {
							std::lock_guard lock(jobsMutex);
							eventsJob.push(event.second.callback);
						}
					}
					break;
					default:
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
			});
	}

	~EventHandler() {
		isEventsRunning = false;
		eventsThread.join();
	}
	void Once(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OneTime, trigger, event, elseEvent });
	}
	void On(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr) {
		if (!batchBegined) 
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent });
		else 
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Trigger, trigger, event, elseEvent });
	}
	void OnChange(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_OnChange, trigger, event, elseEvent });
	}
	void While(std::string eventName, std::shared_ptr<Trigger> trigger, Event event, Event elseEvent = nullptr) {
		if (!batchBegined)
			eventsSnapshot.Insert(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent });
		else
			batch.insert_or_assign(eventName, EventData{ eventName, ET_Continuous, trigger, event, elseEvent });
	}
	void BeginBatch() {
		batch.clear();
		batchBegined = true;
	}
	void EndBatch() {
		eventsSnapshot.Publish(batch);
		batchBegined = false;
	}

	void HandleJobs() {
		std::queue<Event> localJobs;
		{
			std::lock_guard lock(jobsMutex);
			std::swap(localJobs, eventsJob);
		}

		while (!localJobs.empty()) {
			auto& event = localJobs.front();

			event();

			localJobs.pop();
		}
	}
};