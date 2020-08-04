// This sample code is intended to show that the Connext DDS Micro 2.4.12
// C API can be called from a C++ application. It is STRICTLY an example and
// NOT intended to represent production-quality code.

#include <stdio.h>

// headers from Connext DDS Micro installation
#include "rti_me_c.h"
#include "disc_dpse/disc_dpse_dpsediscovery.h"
#include "wh_sm/wh_sm_history.h"
#include "rh_sm/rh_sm_history.h"
#include "netio/netio_udp.h"

// rtiddsgen generated headers
#include "example.h"
#include "examplePlugin.h"
#include "exampleSupport.h"


int main(void)
{
    // user-configurable values
    char *peer = "127.0.0.1";
    char *loopback_name = "Loopback Pseudo-Interface 1";
    char *eth_nic_name = "Wireless LAN adapter Wi-Fi";
    char *local_participant_name = "publisher";
    char *remote_participant_name = "subscriber";
    int domain_id = 100;

    DDS_DomainParticipantFactory *dpf = NULL;
    struct DDS_DomainParticipantFactoryQos dpf_qos = 
            DDS_DomainParticipantFactoryQos_INITIALIZER;
    DDS_DomainParticipant *dp = NULL;
    struct DDS_DomainParticipantQos dp_qos = 
            DDS_DomainParticipantQos_INITIALIZER;
    struct DPSE_DiscoveryPluginProperty discovery_plugin_properties =
            DPSE_DiscoveryPluginProperty_INITIALIZER;
    RT_Registry_T *registry = NULL;
    struct UDP_InterfaceFactoryProperty *udp_property = NULL;
    DDS_Topic *topic = NULL;
    char topic_name[255];
    DDS_Publisher *publisher = NULL;
    DDS_DataWriter *datawriter = NULL;
    struct DDS_DataWriterQos dw_qos = DDS_DataWriterQos_INITIALIZER;
    my_type *sample = NULL;
    my_typeDataWriter *narrowed_datawriter = NULL;
    struct DDS_SubscriptionBuiltinTopicData rem_subscription_data =
            DDS_SubscriptionBuiltinTopicData_INITIALIZER;
    DDS_Entity *entity = NULL;
    int sample_count = 0;
    DDS_ReturnCode_t retcode;
    DDS_Boolean success = DDS_BOOLEAN_FALSE;

    // create the DomainParticipantFactory and registry so that we can make some 
    // changes to the default values
    dpf = DDS_DomainParticipantFactory_get_instance();
    registry = DDS_DomainParticipantFactory_get_registry(dpf);

    // register writer history
    if (!RT_Registry_register(
            registry, 
            DDSHST_WRITER_DEFAULT_HISTORY_NAME,
            WHSM_HistoryFactory_get_interface(), 
            NULL, 
            NULL))
    {
        printf("ERROR: failed to register wh\n");
    }
    // register reader history
    if (!RT_Registry_register(
            registry, 
            DDSHST_READER_DEFAULT_HISTORY_NAME,
            RHSM_HistoryFactory_get_interface(), 
            NULL, 
            NULL))
    {
        printf("ERROR: failed to register rh\n");
    }

    // Set up the UDP transport's allowed interfaces. To do this we:
    // (1) unregister the UDP trasport
    // (2) name the allowed interfaces
    // (3) re-register the transport
    if(!RT_Registry_unregister(
            registry, 
            NETIO_DEFAULT_UDP_NAME, 
            NULL, 
            NULL)) 
    {
        printf("ERROR: failed to unregister udp\n");
    }

    udp_property = (struct UDP_InterfaceFactoryProperty *)
            malloc(sizeof(struct UDP_InterfaceFactoryProperty));
    if (udp_property == NULL) {
        printf("ERROR: failed to allocate udp properties\n");
    }
    *udp_property = UDP_INTERFACE_FACTORY_PROPERTY_DEFAULT;

    // For additional allowed interface(s), increase maximum and length, and
    // set interface below:
    REDA_StringSeq_set_maximum(&udp_property->allow_interface,2);
    REDA_StringSeq_set_length(&udp_property->allow_interface,2);
    *REDA_StringSeq_get_reference(&udp_property->allow_interface,0) = 
            DDS_String_dup(loopback_name); 
    *REDA_StringSeq_get_reference(&udp_property->allow_interface,1) = 
            DDS_String_dup(eth_nic_name); 

    if(!RT_Registry_register(
            registry, 
            NETIO_DEFAULT_UDP_NAME,
            UDP_InterfaceFactory_get_interface(),
            (struct RT_ComponentFactoryProperty*)udp_property, NULL))
    {
        printf("ERROR: failed to re-register udp\n");
    } 

    // register the dpse (discovery) component
    if (!RT_Registry_register(
            registry,
            "dpse",
            DPSE_DiscoveryFactory_get_interface(),
            &discovery_plugin_properties._parent, 
            NULL))
    {
        printf("ERROR: failed to register dpse\n");
    }

    // Now that we've finsihed the changes to the registry, we will start 
    // creating DDS entities. By setting autoenable_created_entities to false 
    // until all of the DDS entities are created, we limit all dynamic memory 
    // allocation to happen *before* the point where we enable everything.
    DDS_DomainParticipantFactory_get_qos(dpf, &dpf_qos);
    dpf_qos.entity_factory.autoenable_created_entities = DDS_BOOLEAN_FALSE;
    DDS_DomainParticipantFactory_set_qos(dpf, &dpf_qos);

    // configure discovery prior to creating our DomainParticipant
    if(!RT_ComponentFactoryId_set_name(&dp_qos.discovery.discovery.name, "dpse")) {
        printf("ERROR: failed to set discovery plugin name\n");
    }
    if(!DDS_StringSeq_set_maximum(&dp_qos.discovery.initial_peers, 1)) {
        printf("ERROR: failed to set initial peers maximum\n");
    }
    if (!DDS_StringSeq_set_length(&dp_qos.discovery.initial_peers, 1)) {
        printf("ERROR: failed to set initial peers length\n");
    }
    *DDS_StringSeq_get_reference(&dp_qos.discovery.initial_peers, 0) = 
            DDS_String_dup(peer);

    // configure the DomainParticipant's resource limits... these are just 
    // examples, if there are more remote or local endpoints these values would
    // need to be increased
    dp_qos.resource_limits.max_destination_ports = 32;
    dp_qos.resource_limits.max_receive_ports = 32;
    dp_qos.resource_limits.local_topic_allocation = 1;
    dp_qos.resource_limits.local_type_allocation = 1;
    dp_qos.resource_limits.local_reader_allocation = 1;
    dp_qos.resource_limits.local_writer_allocation = 1;
    dp_qos.resource_limits.remote_participant_allocation = 8;
    dp_qos.resource_limits.remote_reader_allocation = 8;
    dp_qos.resource_limits.remote_writer_allocation = 8;

    //  set the name of the local DomainParticipant
    // (this is required for DPSE discovery)
    strcpy(dp_qos.participant_name.name, local_participant_name);

    // now the DomainParticipant can be created
    dp = DDS_DomainParticipantFactory_create_participant(
            dpf, 
            domain_id,
            &dp_qos, 
            NULL,
            DDS_STATUS_MASK_NONE);
    if(dp == NULL) {
        printf("ERROR: failed to create participant\n");
    }

    // register the type (my_type, from the idl) with the middleware
    retcode = DDS_DomainParticipant_register_type(
            dp,
            my_typeTypePlugin_get_default_type_name(),
            my_typeTypePlugin_get());
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to register type\n");
    }

    // Create the Topic to which we will publish. Note that the name of the 
    // Topic is stored in my_topic_name, which was defined in the IDL 
    topic = DDS_DomainParticipant_create_topic(
            dp,
            my_topic_name, // this constant is defined in the *.idl file
            my_typeTypePlugin_get_default_type_name(),
            &DDS_TOPIC_QOS_DEFAULT, 
            NULL,
            DDS_STATUS_MASK_NONE);
    if(topic == NULL) {
        printf("ERROR: topic == NULL\n");
    }

    // assert remote DomainParticipant
    retcode = DPSE_RemoteParticipant_assert(dp, remote_participant_name);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant\n");
    }

    // create the Publisher
    publisher = DDS_DomainParticipant_create_publisher(
            dp,
            &DDS_PUBLISHER_QOS_DEFAULT,
            NULL,
            DDS_STATUS_MASK_NONE);
    if(publisher == NULL) {
        printf("ERROR: Publisher == NULL\n");
    }

    // Configure the DataWriter's QoS. Note that the 'rtps_object_id' that we 
    // assign to our own DataWriter here needs to be the same number the remote
    // DataReader will configure for its remote peer.
    dw_qos.protocol.rtps_object_id = 100;
    dw_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    dw_qos.resource_limits.max_samples_per_instance = 32;
    dw_qos.resource_limits.max_instances = 2;
    dw_qos.resource_limits.max_samples = dw_qos.resource_limits.max_instances *
            dw_qos.resource_limits.max_samples_per_instance;
    dw_qos.history.depth = 16;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.sec = 0;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.nanosec = 250000000;

    // now create the DataWriter
    datawriter = DDS_Publisher_create_datawriter(
            publisher, 
            topic, 
            &dw_qos,
            NULL,
            DDS_STATUS_MASK_NONE);
    if(datawriter == NULL) {
        printf("ERROR: datawriter == NULL\n");
    }   

    // When we use Dynamic Participant Static Endpoint (DPSE) discovery we must
    // setup information about any DataReaders we are expecting to discover. 
    // This information includes a unique object ID for the remote peer, as well
    // as its Topic, type, and QoS. 
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 200;
    rem_subscription_data.topic_name = DDS_String_dup(my_topic_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Now we can assert that a remote DomainParticipant (with the name held in
    // remote_participant_name) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            remote_participant_name,
            &rem_subscription_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote publication\n");
    }    

    // create the data sample that we will write
    sample = my_type_create();
    if(sample == NULL) {
        printf("ERROR: failed my_type_create\n");
    }

    // Finaly, now that all of the entities are created, we can enable them all
    entity = DDS_DomainParticipant_as_entity(dp);
    retcode = DDS_Entity_enable(entity);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to enable entity\n");
    }

    // A DDS_DataWriter is not type-specific, thus we need to cast, or "narrow"
    // the DataWriter before we use it to write our samples
    narrowed_datawriter = my_typeDataWriter_narrow(datawriter);
    while (1) {
        
        // add some data to the sample
        sample->id = 1; // arbitrary value
        sprintf(sample->msg, "sample #%d\n", sample_count);

        retcode = my_typeDataWriter_write(
                narrowed_datawriter, 
                sample, 
                &DDS_HANDLE_NIL);
        if(retcode != DDS_RETCODE_OK) {
            printf("ERROR: Failed to write sample\n");
        } else {
            printf("Wrote sample %d\n", sample_count); 
            sample_count++;
        } 
        OSAPI_Thread_sleep(1000); // sleep 1s between writes 
    }
}

