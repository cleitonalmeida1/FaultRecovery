
#ifndef STATEMACHINELIB_STATEMACHINE_H
#define STATEMACHINELIB_STATEMACHINE_H

#include "mbed.h"
#include "ConfigFile.h"
#include "Logger.h"
#include <map>
#include <fstream>
#include <iostream>

using namespace std;

/* -----------------------------------------------------------------------------
 * Class Declarations
 * ---------------------------------------------------------------------------*/
class TypeIDFactory;
class State;
class RecoveryPoint;
class StateMachine;
class Application;

/* -----------------------------------------------------------------------------
 * Type Definitions
 * ---------------------------------------------------------------------------*/
typedef int TypeID;

static const char *RECOVERY_FILE = "/local/state.txt";

/* -----------------------------------------------------------------------------
 * Class Definitions
 * ---------------------------------------------------------------------------*/
class TypeIDFactory {
private:

	static TypeID counter;

	TypeIDFactory() {
	}

	~TypeIDFactory() {
	}

public:

	template<typename T>
	static TypeID getID() {
		static TypeID id = counter++;
		return id;
	}
};

class State {
protected:

	State() {
	}

	State(State &orig) {
	}

	virtual ~State() {
	}

public:

	virtual double getTimeLimit() {
		return 0.0;
	}

	virtual void run(StateMachine &sm) = 0;
};

class RecoveryPoint {
protected:

	RecoveryPoint() {
	}

	RecoveryPoint(RecoveryPoint &orig) {
	}

	virtual ~RecoveryPoint() {
	}

public:

	virtual void run(StateMachine &sm) = 0;
};

class StateMachine {
public:

	StateMachine() :
			configFile(RECOVERY_FILE), logger("/local/teste.txt") {
	}

	StateMachine(StateMachine &orig) :
			recoveryPoints(orig.recoveryPoints), states(orig.states), current(
					orig.current), configFile(RECOVERY_FILE), logger("/local/teste.txt") {
	}

	virtual ~StateMachine() {
	}

	template<class R, class S>
	void addRecoveryPoint() {

		TypeID id = TypeIDFactory::getID<S>();
		recoveryPoints[id] = new R;
	}

	template<class S>
	void setToState() {
		StateMachine::current = TypeIDFactory::getID<S>();
		serializeToFlash(current);
		if (states[current] == NULL) {
			addState<S>();
		}
	}

	template<class S>
	void start() {
		setToState<S>();
		run();
	}

	bool recoveryFrom(TypeID id) {
		RecoveryPoint *recoveryPoint = recoveryPoints[id];
		cout << "idRecovery = " << endl;
		if (recoveryPoint == NULL) {
			cout << "NULL" << endl;
			return false;
		}
		recoveryPoints[id]->run(*this);
		return true;
	}

private:

	std::map<TypeID, RecoveryPoint *> recoveryPoints;
	std::map<TypeID, State *> states;
	TypeID current;
	ConfigFile configFile;
	Logger logger;
	Timer t;
	template<class S>
	void addState() {
		TypeID id = TypeIDFactory::getID<S>();
		states[id] = new S;
	}

	void run() {
		static bool RUNNING = true;
		while (RUNNING) {
			t.reset();
			t.start();
			states[current]->run(*this);
			logger.log("%f", t.read());
			t.stop();
		}
	}

	void serializeToFlash(TypeID id) {
		configFile.setInt("estado", id);
		configFile.save();
	}
};

class Application {
protected:

	Application() :
			localFileSystem("local"), configFile(RECOVERY_FILE) {
	}

	//Application(Application &orig) : localFileSystem(orig.localFileSystem) { }

	virtual ~Application() {
	}

public:

	template<class Application>
	static Application* getInstance() {
		return (Application *) instance;
	}


	template<class App>
	static void initialize() {
		cout << "initialize" << endl;
		if (instance != NULL) {
			cout << "Application has already been initialized";
		}

		static StateMachine sm;

		instance = new App;
		instance->setup();
		instance->createRecoveryPoints(sm);

		static bool RESET_BY_FAULT = true;
		bool started = false;
		cout << "RESET_BY_FAULT" << endl;
		if (RESET_BY_FAULT && instance->recovery(sm)) {
			cout << "RECOVERY" << endl;
			started = true;
		}
		if (!started) {
			instance->start(sm);
		}
	}

	TypeID unserializeFromFlash() {
		int a = atoi(configFile.get("estado"));
		cout << "lendo = " << a << endl;
		return a;
	}

protected:

	virtual void setup() {
	}

	virtual void createRecoveryPoints(StateMachine &sm) {
	}

	virtual void start(StateMachine &sm) = 0;

private:

	static Application *instance;

	LocalFileSystem localFileSystem;

	ConfigFile configFile;

	bool recovery(StateMachine &sm) {
		if (configFile.load()) {
			TypeID id = unserializeFromFlash();
			cout << "id = " << id << endl;

			return sm.recoveryFrom(id);
		}
		return false;
	}
};

#endif //STATEMACHINELIB_STATEMACHINE_H
