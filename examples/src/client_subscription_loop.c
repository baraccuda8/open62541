/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

/**
 * Client disconnect handling
 * --------------------------
 * This example shows you how to handle a client disconnect, e.g., if the server
 * is shut down while the client is connected. You just need to call connect
 * again and the client will automatically reconnect.
 *
 * This example is very similar to the tutorial_client_firststeps.c. */

//#include <exception>
//#include <iostream>
//#include <locale>
//#include <typeinfo>

//#include <windows.h>
//#include <windowsx.h>

#include <open62541/client_config_default.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/log_stdout.h>


//#include "common.h"
#include <signal.h>
#include <stdlib.h>

UA_Boolean running = true;

static void stopHandler(int sign)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Received Ctrl-C");
    running = 0;
}


static void handler_currentTimeChanged(UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId, void *monContext, UA_DataValue *value)
{
    if(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DATETIME]))
    {
        UA_DateTime raw_date = *(UA_DateTime *) value->value.data;
        UA_DateTimeStruct dts = UA_DateTime_toStruct(raw_date);
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "date is: %02u-%02u-%04u %02u:%02u:%02u.%03u", dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);
    }
}

static void deleteSubscriptionCallback(UA_Client *client, UA_UInt32 subscriptionId, void *subscriptionContext)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Subscription Id %u was deleted", subscriptionId);
}

static void subscriptionInactivityCallback (UA_Client *client, UA_UInt32 subId, void *subContext)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Inactivity for subscription %u", subId);
}

UA_UInt32 subscriptionId = 0;

static void stateCallback(UA_Client *client, UA_SecureChannelState channelState, UA_SessionState sessionState, UA_StatusCode recoveryStatus)
{
        if(channelState == UA_SECURECHANNELSTATE_CLOSED)    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "The client is disconnected");
        if(channelState == UA_SECURECHANNELSTATE_HEL_SENT)  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for ack");
        if(channelState == UA_SECURECHANNELSTATE_OPN_SENT)  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Waiting for OPN Response");
        if(channelState == UA_SECURECHANNELSTATE_OPEN)      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A SecureChannel to the server is open");

        if(sessionState == UA_SESSIONSTATE_ACTIVATED)       UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "A session with the server is activated");
        if(sessionState == UA_SESSIONSTATE_CLOSED)          UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Session disconnected");
}

UA_UInt32 InitSubscript(UA_Client *client)
{
    UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
    request.requestedPublishingInterval = 1000.0;
    UA_CreateSubscriptionResponse response = UA_Client_Subscriptions_create(client, request, NULL, NULL, deleteSubscriptionCallback);
    if(response.responseHeader.serviceResult == UA_STATUSCODE_GOOD) UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Create subscription succeeded, id %u", response.subscriptionId);
    else
    {
        //bool l = response.responseHeader.serviceResult == UA_STATUSCODE_BADINTERNALERROR;
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Create subscription not succeeded");
        return 0;
    }
    return response.subscriptionId;
}

bool InitMonitor(UA_Client *client, UA_UInt32 subscriptionId)
{
    //UA_NodeId currentTimeNode = UA_NS0ID(SERVER_SERVERSTATUS_CURRENTTIME);
    UA_NodeId currentTimeNode = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERSTATUS_CURRENTTIME);
    //opcua::Node node(client, opcua::VariableId::Server_ServerStatus_CurrentTime);

    UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(currentTimeNode);
    
    UA_MonitoredItemCreateResult monResponse = UA_Client_MonitoredItems_createDataChange(client, subscriptionId, UA_TIMESTAMPSTORETURN_BOTH, monRequest, NULL, handler_currentTimeChanged, NULL);
    if(monResponse.statusCode == UA_STATUSCODE_GOOD)
    {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Monitoring UA_NS0ID_SERVER_SERVERSTATUS_CURRENTTIME', id %u", monResponse.monitoredItemId);
        return true;
    }
    else
        return false;
    return false;
}




int main(void) 
{
    signal(SIGINT, stopHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *cc = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(cc);

    cc->stateCallback = stateCallback;
    cc->subscriptionInactivityCallback = subscriptionInactivityCallback;

    //while(running)
    {
        UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://192.168.5.158:4840");
        
        if(retval != UA_STATUSCODE_GOOD)
        {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Not connected. Retrying to connect in 1 second");
            //sleep_ms(10000);
            UA_sleep_ms(1000);
        }
        else
        {
            //Добавляем Точку субскрибе
            UA_UInt32 subscriptionId = InitSubscript(client);
            //if(!subscriptionId)
                //throw std::runtime_error("Not Init GdiPlus");

                //Добавляем петеменную
                InitMonitor(client, subscriptionId);

                while(running)
                {
                    UA_Client_run_iterate(client, 1000);
                }
        }
    };

    UA_Client_Subscriptions_deleteSingle(client, subscriptionId);
    /* Clean up */
    UA_Client_delete(client); /* Disconnects the client internally */
    return EXIT_SUCCESS;
}
