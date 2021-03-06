
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

//#define TG_FULL_INTF

using namespace std;
using namespace ajn;
using namespace qcc;

class TG_LightProxy;
vector<TG_LightProxy*> g_lights;

BusAttachment* g_bus = NULL;

#define GET_LINE(str) { \
		getline(cin, str); \
		if (str.c_str()[0] == 'q') return ER_FAIL; }

class TG_LightProxy{

  private:
	char* obj_path = NULL;
	ProxyBusObject proxy;
	const InterfaceDescription* ifc;
	SessionId sessionId;

	size_t numMethods;
	const InterfaceDescription::Member** method_members;

	size_t numSignals;
	const InterfaceDescription::Member** signal_members;

	size_t numProps;
	const InterfaceDescription::Property** props = NULL;

	void BuildMembers(const InterfaceDescription::Member** members, size_t mem_size)
	{
		size_t m = 0;
		size_t s = 0;
		for (size_t i = 0; i < mem_size; i++) {
			if (members[i]->memberType == MESSAGE_METHOD_CALL)	this->numMethods++;
			if (members[i]->memberType == MESSAGE_SIGNAL)       this->numSignals++;
		}

		this->method_members = new const InterfaceDescription::Member*[numMethods];
		this->signal_members = new const InterfaceDescription::Member*[numSignals];

		for (size_t i = 0; i < mem_size; i++) {
			if (members[i]->memberType == MESSAGE_METHOD_CALL) {
				this->method_members[m++] = members[i];
			}
			if (members[i]->memberType == MESSAGE_SIGNAL) {
				this->signal_members[s++] = members[i];
			}
		}
	}

  public:
	~TG_LightProxy()
	{
		printf("[MMM] destroy\n");
		delete obj_path;
		delete ifc;
		delete [] method_members;
		delete [] signal_members;
		delete [] props;
	}

	void Initialize(const char* obj_path, ProxyBusObject proxy, const InterfaceDescription* ifc, SessionId sId)
	{
		this->obj_path = new char[strlen(obj_path)];
		strcpy(this->obj_path, obj_path);
		this->proxy = proxy;
		this->ifc = ifc;
		this->sessionId = sId;

		// temporary get all members
		size_t numMembers = this->ifc->GetMembers();
		const InterfaceDescription::Member** members = new const InterfaceDescription::Member*[numMembers];
		ifc->GetMembers(members, numMembers);

		// build method and signal members
		BuildMembers(members, numMembers);

		// properties
		numProps = ifc->GetProperties();
		props = new const InterfaceDescription::Property*[numProps];
		ifc->GetProperties(props, numProps);
	}

	const char* GetObjPath()
	{
		return this->obj_path;
	}

	size_t GetPropsName(const char** propsName)
	{
		for (size_t i = 0; i < numProps; i++) {
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

	void GetSignalMember(const InterfaceDescription::Member** members)
	{
		for (size_t i = 0; i < this->numSignals; i++) {
			members[i] = signal_members[i];
		}
	}

	size_t GetSignalNum()
	{
		return this->numSignals;
	}

	SessionId GetSessionId()
	{
		return sessionId;
	}

	void ShowMethods()
	{
		for (size_t i = 0; i < numMethods; i++) {
			const qcc::String inSig(method_members[i]->signature);
			const qcc::String outSig(method_members[i]->returnSignature);

			if (outSig.empty())
				printf("\tMETHOD: %s(%s)\n", method_members[i]->name.c_str(), inSig.c_str());
			else
				printf("\tMETHOD: %s(%s) -> %s\n", method_members[i]->name.c_str(), inSig.c_str(), outSig.c_str());

		}
	}

	void ShowSignals()
	{
		for (size_t i = 0; i < numSignals; i++) {
			const qcc::String inSig(signal_members[i]->signature);

            printf("\tSIGNAL: %s(%s)\n", signal_members[i]->name.c_str(), inSig.c_str());
        }
	}

	void ShowProperties()
	{
		for (size_t p = 0; p < numProps; p++) {
			printf("\tPROP  : %s (type = %s)", props[p]->name.c_str(), props[p]->signature.c_str());
			MsgArg arg;
			proxy.GetProperty(ifc->GetName(), props[0]->name.c_str(), arg);
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

	void ShowThisObject()
	{
		printf("BusObj = %s, interface = %s\n", this->obj_path, this->ifc->GetName());
		ShowMethods();
		ShowSignals();
		ShowProperties();
	}

	QStatus CallMethods(string inName)
	{
		size_t m = 0;
		char *methodName = new char[inName.length() + 1];
		strcpy(methodName, inName.c_str());
		for (; m < numMethods; m++) {
			if((strncmp(method_members[m]->name.c_str(), methodName, strlen(methodName)) == 0)) {

				size_t argNum = method_members[m]->signature.length();
				MsgArg* args = new MsgArg[argNum];
				string strIn;
				for (size_t i = 0; i < argNum; i++) {
					char sig = method_members[m]->signature.c_str()[i];
					switch(sig) {
					case 'b':
					{
						printf("\tenter t(true) or f(false) > ");
						GET_LINE(strIn);
						char input = strIn.c_str()[0];

						args[i].Set("b", input == 't' ? true : false);
						break;
					}
					case 'u':
					{
						printf("\tenter num [0-255] > ");
						GET_LINE(strIn);
						uint32_t input = atof(strIn.c_str());

						if (input < 0 || input > 255) {
							printf("[err] out of range\n");
							return ER_OK;
						}
						args[i].Set("u", input);
						break;
					}
					default:
						printf("[err] not supported args signature type = %c\n", sig);
						return ER_OK;
					}
				}
				proxy.MethodCall(this->ifc->GetName(), method_members[m]->name.c_str(), args, argNum, 0);
			}
		}
		if (m > numMethods) {
			printf("[err] connnot find method (%s)\n", inName.c_str());
			return ER_FAIL;
		}
		delete [] methodName;
		return ER_OK;
	}
};

class TG_LightListener : 
	public AboutListener,
	public ProxyBusObject::PropertiesChangedListener,
	public SessionListener,
	public MessageReceiver {
	void Announced(const char* busName, uint16_t version, SessionPort port, const MsgArg& objectDescriptionArg, const MsgArg& aboutDataArg)
	{
		QStatus status;
        printf("*********************************************************************************\n");
		printf("Announce signal discovered, Bus(%s), Port(%hu), ver(%hu)\n", busName, port, version);
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
		status = g_bus->JoinSession(busName, port, this, sessionId, opts);
		printf("SessionJoined sessionId = %u, status = %s\n", sessionId, QCC_StatusText(status));

		// Introspect: refer src [Mcmd.cc router/test]
		for (size_t i = 0; i < path_num; ++i) {
			printf("\n");
			ProxyBusObject robj(*g_bus, busName, paths[i], sessionId);
			#ifdef TG_FULL_INTF
			robj.AddInterface(org::freedesktop::DBus::InterfaceName);
			robj.AddInterface(org::freedesktop::DBus::Introspectable::InterfaceName);
			#endif
			status = robj.IntrospectRemoteObject();
			if (status != ER_OK) {
				printf("IntrospectRemoteObject fail\n");
				return;
			}

			#ifdef TG_FULL_INTF
			size_t numIfaces = robj.GetInterfaces();
			const InterfaceDescription** ifaces = new const InterfaceDescription*[numIfaces];
			robj.GetInterfaces(ifaces, numIfaces);
			#else
			size_t numIfaces = aod.GetInterfaces(paths[i], NULL, 0);
			const char **intfs = new const char*[numIfaces];
			aod.GetInterfaces(paths[i], intfs, numIfaces);
			#endif

			for (size_t j = 0; j < numIfaces; j++) {
				TG_LightProxy* light = new TG_LightProxy();
				g_lights.push_back(light);
				#ifdef TG_FULL_INTF
				const InterfaceDescription* ifc = ifaces[j];
				light->Initialize(paths[i], robj, ifc, sessionId);
				#else
				const InterfaceDescription* ifc = g_bus->GetInterface(intfs[j]);
				light->Initialize(paths[i], robj, ifc, sessionId);
				#endif

				// register listener for property change
				size_t numProps = ifc->GetProperties();
				const char** propName = new const char*[numProps];
				light->GetPropsName(propName);
				robj.RegisterPropertiesChangedListener(ifc->GetName(), propName, numProps, *this, NULL);

				// register signal handler
				size_t numSignals = light->GetSignalNum();
				const InterfaceDescription::Member** sigmem = new const InterfaceDescription::Member*[numSignals];
				light->GetSignalMember(sigmem);
				for (size_t m = 0; m < numSignals; m++) {
					g_bus->RegisterSignalHandler(this, 
									static_cast<MessageReceiver::SignalHandler>(&TG_LightListener::LightSignalHandler), 
									sigmem[m], NULL);
				}

				// print intformation
				printf("==================================================\n");
				light->ShowThisObject();
                printf("==================================================\n");

			}
			#ifdef TG_FULL_INTF
			delete [] ifaces;
			#else
			delete [] intfs;
			#endif
		}
		delete [] paths;
	}	// end of Annouced()

	void LightSignalHandler(const InterfaceDescription::Member* member, const char* path, Message& message)
	{
		QCC_UNUSED(member);
		//QCC_UNUSED(path);
		QCC_UNUSED(message);
		printf("[N] Signal from path = %s, name= %s\n", path, member->name.c_str());
	}

	virtual void PropertiesChanged(ProxyBusObject& obj,
						const char* ifaceName,
						const MsgArg& changed,
						const MsgArg& invalidate,
						void* context) {
		QCC_UNUSED(ifaceName);
		QCC_UNUSED(context);
		//QCC_UNUSED(obj);
		QCC_UNUSED(invalidate);

		QStatus status = ER_OK;
		size_t i = 0;

		printf("\t[N] property changed of (%s)\n", obj.GetPath().c_str());
		g_bus->EnableConcurrentCallbacks();

		// find light object
		TG_LightProxy *light;
		for (i = 0; i < (g_lights.size() - 1); i++) {
			light = g_lights[i];
			if (strcmp(light->GetObjPath(), obj.GetPath().c_str()) == 0)
				break;
		}
		if (i == g_lights.size()) {
			printf("[err] can not find light object\n");
			return;
		}

		size_t nelem = 0;
		MsgArg* elems = NULL;
		status = changed.Get("a{sv}", &nelem, &elems);
		if (ER_OK != status)	return;

		for (i = 0; i < nelem; ++i) {
			const char* prop;
			MsgArg* val;
			const char* propSig;
			status = elems[i].Get("{sv}", &prop, &val);
			if (ER_OK != status) return;

			propSig = light->GetPropSig(prop);
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
	} // end of PropertiesChanged

	void SessionLost(SessionId sessionId, SessionLostReason reason)
	{
		printf("SessionLost sessionId = %u, Reason = %d\n", sessionId, reason);

#if 1
/*
		vector<TG_LightProxy*>::iterator it = g_lights.begin();
		for (size_t i = 0; i < g_lights.size(); i++, it++) {
			TG_LightProxy *light = *it;
			if (light->GetSessionId() == sessionId) {
				printf("[MMM] delete obj [%zu]\n", i);
				//int pos = distance(g_lights.begin(), it);
				//printf("[MMM] distance = %d\n", pos);
				//g_lights.erase(g_lights.at(i));
				g_lights = g_lights.erase(it);
			}
		}
*/
#else
		vector<TG_LightProxy*>::iterator it = g_lights.begin();
		while (it != g_lights.end()) {
			TG_LightProxy *light = *it;
			if (light->GetSessionId() == sessionId) {
				printf("[MMM] delete obj\n");
				//g_lights.erase(*it);
			}
			it++;
		}
#endif	
	}
};


static QStatus CallLightMethod()
{
	QStatus status = ER_OK;
    string input;

	while(g_lights.size() == 0) {
		usleep(100 * 1000);
	}

	printf("Select obj num [0 - %zu] > \n", g_lights.size() - 1);
	GET_LINE(input);

	uint32_t i = atof(input.c_str());
	if ( i < 0 || i >= g_lights.size())
		i = 0;

	TG_LightProxy* light = g_lights[i];
	light->ShowThisObject();

	cout << "Enter Method > ";
	GET_LINE(input);
	status = light->CallMethods(input);
	return status;
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

	QStatus status;
	g_bus = new BusAttachment("light_controller", true);
	status = g_bus->Start();
	assert(ER_OK == status);
	status = g_bus->Connect();
	assert(ER_OK == status);

	TG_LightListener* lightListener = new TG_LightListener();
	g_bus->RegisterAboutListener(*lightListener);

	status = g_bus->WhoImplements(NULL);

	if (ER_OK != status) {
		printf("WhoImplements error\n");
		goto exit;
	}

	while (ER_OK == status) {
		status = CallLightMethod();
	}

exit:
	g_bus->UnregisterAboutListener(*lightListener);
	//delete g_lights;
	delete lightListener;
	delete g_bus;

#ifdef ROUTER
	AllJoynRouterShutdown();
#endif
	AllJoynShutdown();
}
