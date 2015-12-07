
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

size_t g_intf_num = 0;
const char** g_intfs = NULL;

class TGAboutListener : public AboutListener {
	void Announced(const char* busName, uint16_t version, SessionPort port, const MsgArg& objectDescriptionArg, const MsgArg& aboutDataArg)
	{
		QStatus status;
        printf("*********************************************************************************\n");
		printf("Announce signal discovered, Bus(%s), Port(%hu), ver(%hu)\n", busName, port, version);
		AboutData aboutData(aboutDataArg);		// prevent build error. (unused variable)

		if(g_bus != NULL) {

			//AboutOjectDescription objectDescription;
			//objectDescription.CreateFromMsgArg(objectDescriptionArg);

			// Get Object Path
			AboutObjectDescription aod(objectDescriptionArg);

			// Get interface num
			size_t path_num = aod.GetPaths(NULL, 0);
			const char** paths = new const char*[path_num];
			aod.GetPaths(paths, path_num);
			for (size_t i = 0; i < path_num; ++i) {
				g_intf_num += aod.GetInterfaces(paths[i], NULL, 0);
			}
			//printf("[MMM] interface num (%d) found\n", g_intf_num);

			// Get interface name
			g_intfs = new const char*[g_intf_num];
			size_t index = 0;
			for (size_t i = 0; i < path_num; ++i) {
				size_t intf_num = aod.GetInterfaces(paths[i], NULL, 0);
				//printf("[MMM] index (%d)\n", index);
				aod.GetInterfaces(paths[i], &g_intfs[index], intf_num);
				index += intf_num;
			}

			// check interfaces
			#if 0
			for (size_t i = 0; i < g_intf_num; ++i) {
				printf("[MMM] [%d] intf : %s\n", i, g_intfs[i]);
			}
			#endif
			
			// JoinSession
			SessionId sessionId;
			SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
			g_bus->EnableConcurrentCallbacks();
			status = g_bus->JoinSession(busName, port, NULL, sessionId, opts);
			printf("SessionJoined sessionId = %u, status = %s\n", sessionId, QCC_StatusText(status));

			// Introspect
			#if 0
			for (size_t i = 0; i < g_intf_num; i++) {
				const InterfaceDescription* ifc = g_bus->GetInterface(g_intfs[i]);
				if (ifc == NULL) {
					printf("[MMM] ifc for (%s) is null when it should not have been\n", g_intfs[i]);
				}
				//ifc->GetMembers();
				//size_t numMembers = ifc->GetMembers();
				//printf("[MMM] numMembers %d\n", numMembers);
			}
			#endif

			#if 1 // [[ working good
			for (size_t i = 0; i < path_num; ++i) {
				//ProxyBusObject proxyObject(*g_bus, busName, paths[i], sessionId);
				ProxyBusObject robj(*g_bus, busName, paths[i], 0);	// [TODO]this case, remove JoinSession
				// [[ below src from Mcmd.cc "alljoyn_core/router/test"
				//robj.AddInterface(org::freedesktop::DBus::InterfaceName);
				//robj.AddInterface(org::freedesktop::DBus::Introspectable::InterfaceName);
				status = robj.IntrospectRemoteObject();
				if (status == ER_OK) {
					size_t numIfaces = robj.GetInterfaces();
					const InterfaceDescription** ifaces = new const InterfaceDescription*[numIfaces];
					robj.GetInterfaces(ifaces, numIfaces);

					for (size_t j = 0; j < numIfaces; j++) {
						const InterfaceDescription* ifc = ifaces[j];
						size_t numMembers = ifc->GetMembers();
						//printf("[MMM] mem = %d\n", numMembers);
						const InterfaceDescription::Member** members = new const InterfaceDescription::Member*[numMembers];
						ifc->GetMembers(members, numMembers);

						for (size_t m = 0; m < numMembers; m++) {
							if (members[m]->memberType == MESSAGE_METHOD_CALL) {
								const qcc::String inSig(members[m]->signature);
								const qcc::String outSig(members[m]->returnSignature);
								if (outSig.empty())
									printf("METHOD: %s.%s(%s)\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str());
								else
									printf("METHOD: %s.%s(%s) -> %s\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str(), outSig.c_str());
							}
						}
					}
				}
			}
			#endif // ]] working good
		}
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
