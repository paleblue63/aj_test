
#include <alljoyn/Status.h>
#include <alljoyn/BusAttachment.h>
//#include <alljoyn/Observer.h>
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

//#define INTF_NAME "com.tgnine.Light"

using namespace std;
using namespace ajn;
using namespace qcc;

static volatile sig_atomic_t s_interrupt = false;

static void CDECL_CALL SigIntHandler(int sig) {
	QCC_UNUSED(sig);
	s_interrupt = true;
}

BusAttachment* g_bus = NULL;

class TGAboutListener : public AboutListener {
	void Announced(const char* busName, uint16_t version, SessionPort port, const MsgArg& objectDescriptionArg, const MsgArg& aboutDataArg)
	{
		QStatus status;
        printf("*********************************************************************************\n");
		printf("Announce signal discovered, Bus(%s), Port(%hu), ver(%hu)\n", busName, port, version);
		AboutData aboutData(aboutDataArg);		// prevent build error. (unused variable)

		if(g_bus == NULL) {
			printf("g_bus is NULL\n");
			return;
		}
		// Get Object Path
		AboutObjectDescription aod(objectDescriptionArg);

		size_t path_num = aod.GetPaths(NULL, 0);
		const char** paths = new const char*[path_num];
		aod.GetPaths(paths, path_num);

		// JoinSession
		SessionId sessionId;
		SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
		g_bus->EnableConcurrentCallbacks();
		status = g_bus->JoinSession(busName, port, NULL, sessionId, opts);
		printf("SessionJoined sessionId = %u, status = %s\n", sessionId, QCC_StatusText(status));

		// Introspect
		for (size_t i = 0; i < path_num; ++i) {
			ProxyBusObject robj(*g_bus, busName, paths[i], sessionId);
			//ProxyBusObject robj(*g_bus, busName, paths[i], 0);
			// [[ below src from Mcmd.cc "alljoyn_core/router/test"
			//robj.AddInterface(org::freedesktop::DBus::InterfaceName);
			//robj.AddInterface(org::freedesktop::DBus::Introspectable::InterfaceName);
			status = robj.IntrospectRemoteObject();
			if (status != ER_OK) {
				printf("IntrospectRemoteObject fail\n");
				return;
			}

			size_t numIfaces = robj.GetInterfaces();
			const InterfaceDescription** ifaces = new const InterfaceDescription*[numIfaces];
			robj.GetInterfaces(ifaces, numIfaces);

			for (size_t j = 0; j < numIfaces; j++) {
				const InterfaceDescription* ifc = ifaces[j];
				size_t numMembers = ifc->GetMembers();
				const InterfaceDescription::Member** members = new const InterfaceDescription::Member*[numMembers];
				ifc->GetMembers(members, numMembers);

				for (size_t m = 0; m < numMembers; m++) {
					if (members[m]->memberType == MESSAGE_METHOD_CALL) {
						const qcc::String inSig(members[m]->signature);
						const qcc::String outSig(members[m]->returnSignature);
						if (outSig.empty())
							printf("METHOD: ifc = %s, mName = %s(%s)\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str());
						else
							printf("METHOD: ifc = %s, mName = %s(%s) -> %s\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str(), outSig.c_str());
					} else if (members[m]->memberType == MESSAGE_SIGNAL) {
						const qcc::String inSig(members[m]->signature);
						printf("SIGNAL: ifc = %s, mName = %s\n", ifc->GetName(), members[m]->name.c_str());
					} else {
						printf("[MMM] others = %d, mName = %s\n", members[m]->memberType, members[m]->name.c_str());
					}
				}
			}
		}
	}	// end of Annouced()
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
	g_bus = new BusAttachment("light_controller", true);
	status = g_bus->Start();
	assert(ER_OK == status);
	status = g_bus->Connect();
	assert(ER_OK == status);

	TGAboutListener* aboutListener = new TGAboutListener();
	g_bus->RegisterAboutListener(*aboutListener);

	status = g_bus->WhoImplements(NULL);

	if (ER_OK == status) {
		while (s_interrupt == false)
			usleep(100 * 1000);
	}

	g_bus->UnregisterAboutListener(*aboutListener);
	delete aboutListener;
	delete g_bus;

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}
