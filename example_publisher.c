/*
* (c) Copyright, Real-Time Innovations, 2021.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

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

// DPSE Discovery-related constants defined in this header
#include "discovery_constants.h"
// Names of network interfaces on the target machine
#include "nic_config.h"

#define ADMINCONSOLE

void on_publication_matched(
        void *listener_data,
        DDS_DataWriter * writer,
        const struct DDS_PublicationMatchedStatus *status) {

    (void)(listener_data);  // to suppress -Wunused-parameter warning

    DDS_Topic* the_topic;
    char the_topic_name[64];
    
    the_topic = DDS_DataWriter_get_topic(writer);
    strcpy(the_topic_name, DDS_TopicDescription_get_name(DDS_Topic_as_topicdescription(the_topic)));	
    if (status->current_count_change > 0) {
        printf("INFO: Matched a DataReader on Topic: '%s'\n", the_topic_name);
    } else if (status->current_count_change < 0) {
        printf("INFO: Unmatched a DataReader on Topic: '%s'\n", the_topic_name);
    }
}

int main(void)
{
    DDS_ReturnCode_t retcode;

    // create the DomainParticipantFactory and registry so that we can change  
    // some of the default values
    DDS_DomainParticipantFactory *dpf = NULL;
    dpf = DDS_DomainParticipantFactory_get_instance();
    RT_Registry_T *registry = NULL;
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
    // (1) unregister the UDP transport
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

    struct UDP_InterfaceFactoryProperty *udp_property = NULL;
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

#if 0  

    // When you are working on an RTOS or other lightweight OS, the middleware
    // may not be able to get the NIC information automatically. In that case, 
    // the code below can be used to manually tell the middleware about an 
    // interface. The interface name ("en0" below) could be anything, but should
    // match what you included in the "allow_interface" calls above.

    if (!UDP_InterfaceTable_add_entry(
    		&udp_property->if_table,
            0xc0a864c8,	// IP address of 192.168.100.200
			0xffffff00, // netmask of 255.255.255.0
			"en0",
			UDP_INTERFACE_INTERFACE_UP_FLAG |
			UDP_INTERFACE_INTERFACE_MULTICAST_FLAG)) {

    	LOG(1, "failed to add interface")

    }

#endif

    if(!RT_Registry_register(
            registry, 
            NETIO_DEFAULT_UDP_NAME,
            UDP_InterfaceFactory_get_interface(),
            (struct RT_ComponentFactoryProperty*)udp_property, NULL))
    {
        printf("ERROR: failed to re-register udp\n");
    } 

    struct DPSE_DiscoveryPluginProperty discovery_plugin_properties =
            DPSE_DiscoveryPluginProperty_INITIALIZER;
    struct DDS_Duration_t my_lease = {5,0};
    struct DDS_Duration_t my_assert_period = {2,0};
    discovery_plugin_properties.participant_liveliness_lease_duration = my_lease;
    discovery_plugin_properties.participant_liveliness_assert_period = my_assert_period;

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

    // Now that we've finished the changes to the registry, we will start 
    // creating DDS entities. By setting autoenable_created_entities to false 
    // until all of the DDS entities are created, we limit all dynamic memory 
    // allocation to happen *before* the point where we enable everything.
    struct DDS_DomainParticipantFactoryQos dpf_qos = 
            DDS_DomainParticipantFactoryQos_INITIALIZER;
    DDS_DomainParticipantFactory_get_qos(dpf, &dpf_qos);
    dpf_qos.entity_factory.autoenable_created_entities = DDS_BOOLEAN_FALSE;
    DDS_DomainParticipantFactory_set_qos(dpf, &dpf_qos);

    struct DDS_DomainParticipantQos dp_qos = 
            DDS_DomainParticipantQos_INITIALIZER;
    
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
    dp_qos.resource_limits.max_destination_ports = 8;
    dp_qos.resource_limits.max_receive_ports = 8;
    dp_qos.resource_limits.local_topic_allocation = 1;
    dp_qos.resource_limits.local_type_allocation = 1;
    dp_qos.resource_limits.local_reader_allocation = 1;
    dp_qos.resource_limits.local_writer_allocation = 1;
#ifdef ADMINCONSOLE
    dp_qos.resource_limits.remote_participant_allocation = 5;  
    dp_qos.resource_limits.remote_reader_allocation = 4; 
    dp_qos.resource_limits.remote_writer_allocation = 2; 
    dp_qos.resource_limits.matching_writer_reader_pair_allocation = 128;
#else
    dp_qos.resource_limits.remote_participant_allocation = 3;
    dp_qos.resource_limits.remote_reader_allocation = 3;
    dp_qos.resource_limits.remote_writer_allocation = 2;
#endif

    // set the name of the local DomainParticipant (i.e. - this application) 
    // from the constants defined in discovery_constants.h
    // (this is required for DPSE discovery)
    strcpy(dp_qos.participant_name.name, k_PARTICIPANT01_NAME);

    // now the DomainParticipant can be created
    DDS_DomainParticipant *dp = NULL;
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
    DDS_Topic *topic = NULL;
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

    // assert the remote DomainParticipants (who's names are defined in 
    // discovery_constants.h) that we are expecting to discover
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT02_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant 2\n");
    }
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT03_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant 3\n");
    }
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT04_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant 4\n");
    }
#ifdef ADMINCONSOLE
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT_ADMINCONSOLE_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert Admin Console\n");
    }
#endif

    // create the Publisher
    DDS_Publisher *publisher = NULL;
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
    // DataReader will configure for its remote peer. We are defining these IDs
    // and other constants in discovery_constants.h
    struct DDS_DataWriterQos dw_qos = DDS_DataWriterQos_INITIALIZER;
    dw_qos.protocol.rtps_object_id = k_OBJ_ID_PARTICIPANT01_DW01;
    dw_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#ifdef ADMINCONSOLE
    dw_qos.writer_resource_limits.max_remote_readers = 4;
#else    
    dw_qos.writer_resource_limits.max_remote_readers = 3;
#endif
    dw_qos.resource_limits.max_samples_per_instance = 16;
    dw_qos.resource_limits.max_instances = 2;
    dw_qos.resource_limits.max_samples = dw_qos.resource_limits.max_instances *
            dw_qos.resource_limits.max_samples_per_instance;
    dw_qos.history.depth = 16;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.sec = 0;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.nanosec = 250000000;

    // Configure a deadline. In the case of a DataWriter, this is the period in 
    // which we are "promising" to send >=1 sample for each instance we are 
    // updating. (1.5s here, as an example)
    dw_qos.deadline.period.sec = 1;
    dw_qos.deadline.period.nanosec = 500000000;

    struct DDS_DataWriterListener dw_listener = DDS_DataWriterListener_INITIALIZER;
    dw_listener.on_publication_matched = on_publication_matched;

    // now create the DataWriter
    DDS_DataWriter *datawriter = NULL;
    datawriter = DDS_Publisher_create_datawriter(
            publisher, 
            topic, 
            &dw_qos,
            &dw_listener,
            DDS_PUBLICATION_MATCHED_STATUS);
    if(datawriter == NULL) {
        printf("ERROR: datawriter == NULL\n");
    }   

    // When we use DPSE discovery we must manually setup information about any 
    // DataReaders we are expecting to discover, and assert them. In this 
    // example code we will do this for 2 remote DataReaders. This information 
    // includes a unique  object ID for the remote peer (we are defining this in 
    // discovery_constants.h), as well as its Topic, type, and QoS. 

    struct DDS_SubscriptionBuiltinTopicData rem_subscription_data =
            DDS_SubscriptionBuiltinTopicData_INITIALIZER;

    // first remote DataReader
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT02_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(my_topic_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT02_NAME) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT02_NAME,
            &rem_subscription_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote subscription #1\n");
    }  

    // second remote DataReader
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT03_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(my_topic_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT03_NAME) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT03_NAME,
            &rem_subscription_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote subscription #2\n");
    }    

    // third remote DataReader
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT04_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(my_topic_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT04_NAME) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT04_NAME,
            &rem_subscription_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote subscription #3\n");
    } 
#ifdef ADMINCONSOLE
    // Admin Console's Subscriber
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_ADMINCONSOLE_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(my_topic_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;

    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT_ADMINCONSOLE_NAME,
            &rem_subscription_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote Admin Console subscription\n");
    } 
#endif

    // create the data sample that we will write
    my_type *sample = NULL;
    sample = my_type_create();
    if(sample == NULL) {
        printf("ERROR: failed my_type_create\n");
    }

    // Finally, now that all of the entities are created, we can enable them all
    retcode = DDS_Entity_enable(DDS_DomainParticipant_as_entity(dp));
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to enable entity\n");
    }

    // A DDS_DataWriter is not type-specific, thus we need to cast, or "narrow"
    // the DataWriter before we use it to write our samples
    my_typeDataWriter *narrowed_datawriter = NULL;
    narrowed_datawriter = my_typeDataWriter_narrow(datawriter);

    int sample_count = 0;

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

