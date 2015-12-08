
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

//#define TG_FULL_INTF

using namespace std;
using namespace ajn;
using namespace qcc;

static volatile sig_atomic_t s_interrupt = false;

static void CDECL_CALL SigIntHandler(int sig) {
	QCC_UNUSED(sig);
	s_interrupt = true;
}

BusAttachment* g_bus = NULL;

class TG_LightProxy {

  private:
	ProxyBusObject proxy;
	const InterfaceDescription* ifc;
	char* intf_name = NULL;

	size_t numMembers;
	const InterfaceDescription::Member** members;

  public:
	void SetProxyBusObject(ProxyBusObject proxy)
	{
		this->proxy = proxy;
	}

	void SetIntfName(const char* _intf_name)
	{
		size_t intf_len = strlen(_intf_name);
		this->intf_name = new char[intf_len];
		strcpy(this->intf_name, _intf_name);
		printf("intf_name = %s\n", intf_name);
	}

	void BuildMembers(const InterfaceDescription* ifc)
	{
		this->ifc = ifc;
		numMembers = ifc->GetMembers();
		members = new const InterfaceDescription::Member*[numMembers];
		ifc->GetMembers(members, numMembers);

	}

	void ShowMethods()
	{
        for (size_t m = 0; m < numMembers; m++) {
            if (members[m]->memberType == MESSAGE_METHOD_CALL) {
                const qcc::String inSig(members[m]->signature);
                const qcc::String outSig(members[m]->returnSignature);
                if (outSig.empty())
                    printf("METHOD: ifc = %s, mName = %s(%s)\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str());
                else
                    printf("METHOD: ifc = %s, mName = %s(%s) -> %s\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str(), outSig.c_str());
            }
        }
	}

	void ShowSignals()
	{
		for (size_t m = 0; m < numMembers; m++) {
            if (members[m]->memberType == MESSAGE_SIGNAL) {
                const qcc::String inSig(members[m]->signature);
                printf("SIGNAL: ifc = %s, mName = %s\n", ifc->GetName(), members[m]->name.c_str());
            }
        }
	}

	QStatus CallMethods(string inName)
	{
		size_t m = 0;
		char *methodName = new char[inName.length() +1];
		strcpy(methodName, inName.c_str());
		for (; m < numMembers; m++) {
			if (members[m]->memberType == MESSAGE_METHOD_CALL) {
				if (strcmp(members[m]->name.c_str(), methodName) == 0) {
					proxy.MethodCall(intf_name, members[m]->name.c_str(), NULL, 0);
					printf("[MMM] invoke mothod (%s)\n", inName.c_str());
				}
			}
		}
		if (m > numMembers) {
			printf("[MMM] connnot found (%s)\n", inName.c_str());
		}
		delete [] methodName;
		return ER_OK;
	}
};

static TG_LightProxy* g_lightProxy = NULL;

class TG_LightListener : public AboutListener {
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

		// Introspect: refer src [Mcmd.cc router/test]
		for (size_t i = 0; i < path_num; ++i) {
			printf("\n");
			ProxyBusObject robj(*g_bus, busName, paths[i], sessionId);
			//ProxyBusObject robj(*g_bus, busName, paths[i], 0);
			status = robj.IntrospectRemoteObject();
			if (status != ER_OK) {
				printf("IntrospectRemoteObject fail\n");
				return;
			}

			g_lightProxy->SetProxyBusObject(robj);

			size_t numIfaces = aod.GetInterfaces(paths[i], NULL, 0);
			const char **intfs = new const char*[numIfaces];
			aod.GetInterfaces(paths[i], intfs, numIfaces);

			for (size_t j = 0; j < 1/*numIfaces*/; j++) {
				const InterfaceDescription* ifc = g_bus->GetInterface(intfs[j]);
				if (g_lightProxy != NULL) {
					g_lightProxy->SetIntfName(intfs[j]);
				}

				// get properties
				#if 0
				MsgArg props;
				robj.GetAllProperties(intfs[j], props);
				printf("\t[MMM] prop ++\n%s\n", props.ToString().c_str());
				printf("\t[MMM] prop --\n");
				#endif

				if (g_lightProxy != NULL)
					g_lightProxy->BuildMembers(ifc);

			}
			#if TG_FULL_INTF
			delete [] ifaces;
			#else
			delete [] intfs;
			#endif
		}
		delete [] paths;
	}	// end of Annouced()
};


static void CallLightMethod()
{
	g_lightProxy->ShowMethods();

	//bool done = false;
	//while(!done) {
        string input;
        cout << ">";
        getline(cin, input);
        g_lightProxy->CallMethods(input);
    //}
}

static bool Parse(const string& input)
{
	char cmd;
	size_t argpos;
	string arg = "";

	if (input.length() == 0) {
		return true;
	}

	cmd = input[0];
	argpos = input.find_first_not_of("\t", 1);
	if (argpos != input.npos) {
		arg = input.substr(argpos);
	}

	switch(cmd) {
	case 'q':
		return false;
	#if 0
	case 'm':
		ListLightMethod();
		break;
	case 's':
		ListLightSignal();
		break;
	case 'p':
		ListLightProperty();
		break;
	#endif
	case 'c':
		CallLightMethod();
		break;
	default:
		return false;
		//break;
	}
	return true;
}

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

	g_lightProxy = new TG_LightProxy();

	TG_LightListener* lightListener = new TG_LightListener();
	g_bus->RegisterAboutListener(*lightListener);

	status = g_bus->WhoImplements(NULL);

	bool done = false;
	if (ER_OK != status) {
		printf("WhoImplements error\n");
		goto exit;
	}

	while(!done) {
		string input;
		cout << "> ";
		getline(cin, input);
		done = !Parse(input);
	}

exit:
	g_bus->UnregisterAboutListener(*lightListener);
	delete g_lightProxy;
	delete lightListener;
	delete g_bus;

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}
