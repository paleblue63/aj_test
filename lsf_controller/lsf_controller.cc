#include <qcc/Thread.h>
#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/Init.h>

// added for About
#include <alljoyn/AboutData.h>
#include <alljoyn/AboutListener.h>
#include <alljoyn/AboutObjectDescription.h>
#include <alljoyn/AboutProxy.h>

// added for Session
#include <alljoyn/Session.h>
#include <alljoyn/SessionListener.h>

#include <signal.h>
#include <vector>

// added for Log Level change
#include <qcc/Log.h>


// TODO: Menu Step

using namespace std;
using namespace ajn;
using namespace qcc;

static volatile sig_atomic_t g_update = false;
static volatile sig_atomic_t s_interrupt = false;

static void CDECL_CALL SigIntHandler(int sig)
{
	QCC_UNUSED(sig);
	s_interrupt = true;
}

BusAttachment* g_bus = NULL;

class TG_LampConnection;
typedef std::map<qcc::String, TG_LampConnection*> LampMap;
LampMap aboutsList;

#define GET_LINE(str) { \
		getline(cin, str); \
		if (str.c_str()[0] == 'q') return ER_FAIL; }

const char* LampServiceObjectPath = "/org/allseen/LSF/Lamp";
const char* LampServiceStateInterfaceName = "org.allseen.LSF.LampState";

//const char* ConfigServiceObjectPath = "/Config";

//--------------------------------------------------------------
// [[ Lamp Connection
class TG_LampConnection {
public:
	qcc::String lampId;
	ProxyBusObject object;
	//ajn::ProxyBusObject configObject;
	AboutProxy* aboutObject;
	qcc::String busName;
	SessionPort port;
	SessionId sessionID;

public:
	TG_LampConnection() {
		lampId = "";
		busName = "";
		port = 0;
		aboutObject = NULL;
		ClearSessionAndObjects();
	}

	~TG_LampConnection() {
		printf("[MMM] disstory\n");
	}

	void Set(qcc::String& lampid, qcc::String& busname, SessionPort& sessionPort) {
		lampId = lampid;
		busName = busname;
		port = sessionPort;
	}

	void ClearSessionAndObjects(void) {
		sessionID = 0;
		object = ProxyBusObject();
		//configObject = ProxyBusObject();
		if (aboutObject) {
			delete aboutObject;
			aboutObject = NULL;
		}
	}

	void InitializeSessionAndObjects(SessionId sessionId) {
		sessionID = sessionId;
		object = ProxyBusObject(*g_bus, busName.c_str(), LampServiceObjectPath, sessionId);
		//configObject = ProxyBusObject(*g_bus, busName.c_str(), ConfigServiceObjectPath, sessionId);
        aboutObject = new AboutProxy(*g_bus, busName.c_str(), sessionId);
	}
};

// ]] Lamp Connection
//--------------------------------------------------------------

//--------------------------------------------------------------
// [[ listner
class TG_LsfListener :
	public AboutListener,
	public SessionListener {

	void Announced(const char* busName, uint16_t version, SessionPort port, const MsgArg& objectDescriptionArg, const MsgArg& aboutDataArg)
	{
		QCC_UNUSED(version);

		QStatus status;
		printf("*********************************************************************************\n");
		printf("Announce signal discovered, Bus(%s), Port(%hu),", busName, port);

		qcc::String busname = busName;
		printf(" IP(%s)\n", AllJoynRouterGetIP(busname).c_str());

		AboutObjectDescription objectDescs(objectDescriptionArg);
		if (objectDescs.HasPath(LampServiceObjectPath)) {
			AboutData aboutData(aboutDataArg);

			char* uniqueId;
			aboutData.GetDeviceId(&uniqueId);
			qcc::String lampID = uniqueId;

			LampMap::iterator it = aboutsList.find(lampID);
			if (it != aboutsList.end()) {
				printf("%s: We already know this dev\n", __func__);
			} else {
				TG_LampConnection* connection = new TG_LampConnection();
				connection->Set(lampID, busname, port);
				aboutsList.insert(std::make_pair(lampID, connection));

				// Join Session
				SessionId sessionId;
				SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
				g_bus->EnableConcurrentCallbacks();
				status = g_bus->JoinSession(busName, port, this, sessionId, opts);
				printf("SessionJoined sessionId = %u, status = %s\n", sessionId, QCC_StatusText(status));

				// TODO: check g_bus is used or not...
				connection->InitializeSessionAndObjects(sessionId);
				printf("Init lamp ID : %s\n", lampID.c_str());
				g_update = true;
			}
		}
	}

	void SessionLost(SessionId sessionId, SessionLostReason reason)
	{
		printf("SessionLost sessionId = %u, Reason = %d\n", sessionId, reason);
		for (LampMap::iterator it = aboutsList.begin(); it != aboutsList.end(); it++) {
			if (it->second->sessionID == sessionId) {
				aboutsList.erase(it);
				printf("remove object\n");
				g_update = true;
				return;
			}
		}
	}
};
// ]] listner
//--------------------------------------------------------------

class CheckUpdateThread : public Thread, public ThreadListener {
public:
	static CheckUpdateThread* Launch(void)
	{
		CheckUpdateThread *bgThread = new CheckUpdateThread();
		bgThread->Start();

		return bgThread;
	}

	void ThreadExit(Thread* thread)
	{
		printf("Thread exit...\n");
		thread->Join();
		delete thread;
	}

protected:
	ThreadReturn STDCALL Run(void* args)
	{
		QCC_UNUSED(args);
		printf("Thread stated\n");

		//TODO: make semaphore
		while (s_interrupt == false) {
			usleep(100 * 1000);

			if (g_update == true) {
				g_update = false;

				printf("=== %zu lights found\n", aboutsList.size());
				for (LampMap::iterator it = aboutsList.begin(); it != aboutsList.end(); it++) {
					printf(" ID : %s\n", it->second->lampId.c_str());
				}
			}
		}
		printf("Thread ended by INTR\n");
		return 0;
	}
};

int CDECL_CALL main(int argc, char** argv)
{
	QCC_UNUSED(argc);
	QCC_UNUSED(argv);

	if (AllJoynInit() != ER_OK) {
		return EXIT_FAILURE;
	}

#ifdef ROUTER
	if (AllJoynRouterInit() != ER_OK) {
		AllJoynShutdown();
		return EXIT_FAILURE;
	}
#endif

	signal(SIGINT, SigIntHandler);

	QStatus status;
	g_bus = new BusAttachment("lsf_controller", true);
	status = g_bus->Start();
	assert(ER_OK == status);
	status = g_bus->Connect();
	assert(ER_OK == status);

	printf("Busattachment success. My Bus Name = %s\n", g_bus->GetUniqueName().c_str());

	TG_LsfListener* lsfListener = new TG_LsfListener();
	g_bus->RegisterAboutListener(*lsfListener);

	CheckUpdateThread* thread = CheckUpdateThread::Launch();

	status = g_bus->WhoImplements(LampServiceStateInterfaceName);
	if (ER_OK != status) {
		printf("WhoImplements error\n");
		goto exit;
	}

	while (s_interrupt == false) {
		usleep(100 * 1000);
	}

exit:
	//if (thread) CheckUpdateThread::ThreadExit(thread);
	if (thread) thread->ThreadExit(thread);
	g_bus->UnregisterAboutListener(*lsfListener);
	delete lsfListener;
	delete g_bus;

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}
