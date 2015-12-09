
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

#if 1
static volatile sig_atomic_t s_interrupt = false;

static void CDECL_CALL SigIntHandler(int sig) {
	QCC_UNUSED(sig);
	s_interrupt = true;
}
#endif

BusAttachment* g_bus = NULL;

class TG_LightProxy{

  private:
	ProxyBusObject proxy;
	const InterfaceDescription* ifc;
	char* intf_name = NULL;

	size_t numMembers;
	const InterfaceDescription::Member** members;

	//static const char* props[];
	size_t numProps;
	const InterfaceDescription::Property** props = NULL;

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

	void SetIntfDescription(const InterfaceDescription* ifc)
	{
		this->ifc = ifc;
	}

	void BuildMembers()
	{
		//this->ifc = ifc;
		numMembers = ifc->GetMembers();
		members = new const InterfaceDescription::Member*[numMembers];
		ifc->GetMembers(members, numMembers);

	}

	void BuildProps()
	{
		numProps = ifc->GetProperties();
		props = new const InterfaceDescription::Property*[numProps];
		ifc->GetProperties(props, numProps);
	}

	size_t GetPropsName(const char** propsName)
	{
		for (size_t i = 0; i < numProps; i++) {
			//strcpy(propsName[i], props[i]->name.c_str());
			propsName[i] = props[i]->name.c_str();
		}
		return numProps;
	}

	const char* GetPropSig(const char* propsName)
	{
		for (size_t i = 0; i < numProps; i++) {
			if (strcmp(propsName, props[i]->name.c_str()) == 0)
				return props[i]->signature.c_str();
		}
		return NULL;
	}

	void ShowMethods()
	{
        for (size_t m = 0; m < numMembers; m++) {
            if (members[m]->memberType == MESSAGE_METHOD_CALL) {
                const qcc::String inSig(members[m]->signature);
                const qcc::String outSig(members[m]->returnSignature);
                if (outSig.empty())
                    //printf("METHOD: ifc = %s, mName = %s(%s)\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str());
					printf("METHOD: mName = %s(%s)\n", members[m]->name.c_str(), inSig.c_str());
                else
                    //printf("METHOD: ifc = %s, mName = %s(%s) -> %s\n", ifc->GetName(), members[m]->name.c_str(), inSig.c_str(), outSig.c_str());
					printf("METHOD: mName = %s(%s) -> %s\n", members[m]->name.c_str(), inSig.c_str(), outSig.c_str());
            }
        }
	}

	void ShowSignals()
	{
		for (size_t m = 0; m < numMembers; m++) {
            if (members[m]->memberType == MESSAGE_SIGNAL) {
                const qcc::String inSig(members[m]->signature);
                printf("SIGNAL: mName = %s\n", members[m]->name.c_str());
            }
        }
	}

	void ShowProperties()
	{
		for (size_t p = 0; p < numProps; p++) {
			printf("PROP  : mName = %s (type = %s)", props[p]->name.c_str(), props[p]->signature.c_str());
			MsgArg arg;
			proxy.GetProperty(intf_name, props[0]->name.c_str(), arg);
			if (strncmp(props[p]->signature.c_str(), "b", 1) == 0) {
				bool val;
				arg.Get("b", &val);
				printf(" value = %s", val ? "true" : "false");
			} else if (strncmp(props[p]->signature.c_str(), "u", 1) == 0) {
				uint32_t val;
				arg.Get("u", &val);
				printf(" value = %d", val);
			}
			printf("\n");
		}
	}

	QStatus CallMethods(string inName)
	{
		size_t m = 0;
		char *methodName = new char[inName.length() + 1];
		strcpy(methodName, inName.c_str());
		for (; m < numMembers; m++) {
			if ((members[m]->memberType == MESSAGE_METHOD_CALL) &&
				(strncmp(members[m]->name.c_str(), methodName, strlen(methodName)) == 0)) {

				if (!members[m]->signature.empty()) {
					char* strSig = new char[members[m]->signature.length() + 1];
					strcpy(strSig, members[m]->signature.c_str());
					char sig = strSig[0];

					switch(sig) {
					case 'b':
					{
						MsgArg arg;
						string strIn;
						printf("\tenter t(ture) or f(false) > ");
    					getline(cin, strIn);
						char* input = new char[strIn.length() + 1];
						strcpy(input, strIn.c_str());

						arg.Set("b", input[0] == 't' ? true : false);
						proxy.MethodCall(intf_name, members[m]->name.c_str(), &arg, 1, 0);
						break;
					}
					case 'u':
					{
						MsgArg arg;
						string strIn;
						printf("\tenter num [0-100] > ");
						getline(cin, strIn);
						uint32_t input = atof(strIn.c_str());

						if (input >=0 && input <=100) {
							arg.Set("u", input);
							proxy.MethodCall(intf_name, members[m]->name.c_str(), &arg, 1, 0);
						} else {
							printf("[err] out of range\n");
							return ER_OK;
						}
						break;
					}
					default:
						printf("[err] not supported args\n");
						return ER_OK;
					}
				}
			}
		}
		if (m > numMembers) {
			printf("[err] connnot find method (%s)\n", inName.c_str());
			return ER_FAIL;
		}
		delete [] methodName;
		return ER_OK;
	}
};

static TG_LightProxy* g_lightProxy = NULL;

class TG_LightListener : public AboutListener, public ProxyBusObject::PropertiesChangedListener {
	void Announced(const char* busName, uint16_t version, SessionPort port, const MsgArg& objectDescriptionArg, const MsgArg& aboutDataArg)
	{
		QStatus status;
        printf("*********************************************************************************\n");
		printf("Announce signal discovered, Bus(%s), Port(%hu), ver(%hu)\n", busName, port, version);
		//AboutData aboutData(aboutDataArg);		// prevent build error. (unused variable)
		QCC_UNUSED(aboutDataArg);

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
					g_lightProxy->SetIntfDescription(ifc);
                    g_lightProxy->BuildMembers();
					g_lightProxy->BuildProps();

					size_t numProps = ifc->GetProperties();
					const char** propName = new const char*[numProps];
					g_lightProxy->GetPropsName(propName);
					robj.RegisterPropertiesChangedListener(intfs[j], propName, numProps, *this, NULL);

					printf("=========================\n");
					g_lightProxy->ShowMethods();
					g_lightProxy->ShowSignals();
					g_lightProxy->ShowProperties();
					printf("=========================\n");
				}

			}
			#if TG_FULL_INTF
			delete [] ifaces;
			#else
			delete [] intfs;
			#endif
		}
		delete [] paths;
	}	// end of Annouced()

	virtual void PropertiesChanged(ProxyBusObject& obj,
						const char* ifaceName,
						const MsgArg& changed,
						const MsgArg& invalidate,
						void* context) {
		QCC_UNUSED(ifaceName);
		QCC_UNUSED(context);
		QCC_UNUSED(obj);
		QCC_UNUSED(invalidate);

		printf("\tproperty changed *********\n");
		QStatus status = ER_OK;
		g_bus->EnableConcurrentCallbacks();
		
		size_t nelem = 0;
		MsgArg* elems = NULL;
		status = changed.Get("a{sv}", &nelem, &elems);
		if (ER_OK != status)	return;

		for (size_t i = 0; i < nelem; ++i) {
			const char* prop;
			MsgArg* val;
			const char* propSig;
			status = elems[i].Get("{sv}", &prop, &val);
			if (ER_OK != status) return;

			propSig = g_lightProxy->GetPropSig(prop);
			if (propSig == NULL) return;

			char sig = propSig[0];

			printf("\t\tprop name = %s, type = %s, sig = %c, ", prop, propSig, sig);
			switch(sig) {
			case 'b':
			{
				bool bVal;
				status = val->Get("b", &bVal);
				printf("value = %s\n", bVal ? "true" : "false");
				break;
			}
			case 'u':
				uint32_t u32Val;
				status = val->Get("u", &u32Val);
				printf("value = %d\n", u32Val);
				break;
			default:
				printf("\tunknown type signature\n");
				return;
			}
		}
	}
};


static QStatus CallLightMethod()
{
	QStatus status;
    string input;
    //cout << "Enter the method Name >";
    getline(cin, input);
    status = g_lightProxy->CallMethods(input);
	return status;
}

#if 0
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
#endif

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

	//bool done = false;
	if (ER_OK != status) {
		printf("WhoImplements error\n");
		goto exit;
	}

#if 0
	while(!done) {
		string input;
		cout << "> ";
		getline(cin, input);
		done = !Parse(input);
	}
#endif

	while(!s_interrupt && ER_OK == status) {
		status = CallLightMethod();
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
