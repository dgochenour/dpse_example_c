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

void my_typeSubscriber_on_requested_deadline_missed(
        void *listener_data,
        DDS_DataReader * reader,
        const struct DDS_RequestedDeadlineMissedStatus *status) 
{   
    (void)(listener_data);  // to suppress -Wunused-parameter warning
    printf("INFO: Deadline missed! This has happened %d times\n", 
            status->total_count);
    printf("INFO: The last instance handle was: ");
    for (int i = 0; i < 16; i++) {
        printf("%d ", status->last_instance_handle.octet[i]);
    }
    printf("\n");
}

void my_typeSubscriber_on_subscription_matched(
        void *listener_data,
        DDS_DataReader * reader,
        const struct DDS_SubscriptionMatchedStatus *status)
{
    (void)(listener_data);  // to suppress -Wunused-parameter warning

    char the_topic_name[64];
    
    strcpy(the_topic_name, DDS_TopicDescription_get_name(DDS_DataReader_get_topicdescription(reader)));	
    if (status->current_count_change > 0) {
        printf("INFO: Matched a DataWriter on Topic: '%s'\n", the_topic_name);
    } else if (status->current_count_change < 0) {
        printf("INFO: Unmatched a DataWriter on Topic: '%s'\n", the_topic_name);
    }
}

void my_typeSubscriber_on_data_available(
        void *listener_data,
        DDS_DataReader * reader)
{
    (void)(listener_data);  // to suppress -Wunused-parameter warning
    
    my_typeDataReader *narrowed_reader = my_typeDataReader_narrow(reader);
    struct DDS_SampleInfoSeq info_seq = DDS_SEQUENCE_INITIALIZER;
    struct my_typeSeq sample_seq = DDS_SEQUENCE_INITIALIZER;
    const DDS_Long MAX_SAMPLES_PER_TAKE = 32;
    DDS_ReturnCode_t retcode;

    retcode = my_typeDataReader_take(
            narrowed_reader, 
            &sample_seq, 
            &info_seq, 
            MAX_SAMPLES_PER_TAKE, 
            DDS_ANY_SAMPLE_STATE, 
            DDS_ANY_VIEW_STATE, 
            DDS_ANY_INSTANCE_STATE);
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to take data, retcode = %d\n", retcode);
    }

    // print each valid sample taken
    DDS_Long i;
    for (i = 0; i < my_typeSeq_get_length(&sample_seq); ++i) {
        struct DDS_SampleInfo *sample_info = 
                DDS_SampleInfoSeq_get_reference(&info_seq, i);
        if (sample_info->valid_data) {
            my_type *sample = my_typeSeq_get_reference(&sample_seq, i);

            printf("\nValid sample received\n");
            printf("\tsample id = %d\n", sample->id);
            printf("\tsample msg = %s\n", sample->msg);
        } else {
            printf("\nSample received\n\tINVALID DATA\n");
        }
    }

    my_typeDataReader_return_loan(narrowed_reader, &sample_seq, &info_seq);
    my_typeSeq_finalize(&sample_seq);
    DDS_SampleInfoSeq_finalize(&info_seq);
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
    dp_qos.resource_limits.max_destination_ports = 32;
    dp_qos.resource_limits.max_receive_ports = 32;
    dp_qos.resource_limits.local_topic_allocation = 1;
    dp_qos.resource_limits.local_type_allocation = 1;
    dp_qos.resource_limits.local_reader_allocation = 1;
    dp_qos.resource_limits.local_writer_allocation = 1;
    dp_qos.resource_limits.remote_participant_allocation = 8;
    dp_qos.resource_limits.remote_reader_allocation = 8;
    dp_qos.resource_limits.remote_writer_allocation = 8;

    // set the name of the local DomainParticipant (i.e. - this application) 
    // from the constants defined in discovery_constants.h
    // (this is required for DPSE discovery)
    strcpy(dp_qos.participant_name.name, k_PARTICIPANT02_NAME);

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
    // Create the Topic to which we will subscribe. Note that the name of the 
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

    // assert the remote DomainParticipant who's name is held in 
    // the constant k_PARTICIANT01_NAME, defined in discovery_constants.h
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT01_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant\n");
    }

    // create the Subscriber
    DDS_Subscriber *subscriber = NULL;
    subscriber = DDS_DomainParticipant_create_subscriber(
            dp,
            &DDS_SUBSCRIBER_QOS_DEFAULT,
            NULL, 
            DDS_STATUS_MASK_NONE);
    if(subscriber == NULL) {
        printf("ERROR: subscriber == NULL\n");
    }

    // Configure the DataReader's QoS. Note that the 'rtps_object_id' that we 
    // assign to our own DataReader here needs to be the same number the remote
    // DataWriter will configure for its remote peer. We are defining these IDs
    // and other constants in discovery_constants.h
    struct DDS_DataReaderQos dr_qos = DDS_DataReaderQos_INITIALIZER;
    dr_qos.protocol.rtps_object_id = k_OBJ_ID_PARTICIPANT02_DR01;
    dr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    dr_qos.resource_limits.max_instances = 2;
    dr_qos.resource_limits.max_samples_per_instance = 16;
    dr_qos.resource_limits.max_samples = dr_qos.resource_limits.max_instances *
            dr_qos.resource_limits.max_samples_per_instance;
    dr_qos.reader_resource_limits.max_remote_writers = 10;
    dr_qos.reader_resource_limits.max_remote_writers_per_instance = 10;
    dr_qos.history.depth = 16;

    // Configure a deadline. In the case of a DataReader, this is the period in 
    // which we are expecting to receive >=1 sample for each instance of which
    // we are aware. (2.0s here, as an example)
    dr_qos.deadline.period.sec = 2;
    dr_qos.deadline.period.nanosec = 0;

    // Configure the DataReaderListener callback
    struct DDS_DataReaderListener dr_listener = DDS_DataReaderListener_INITIALIZER;
    dr_listener.on_data_available = my_typeSubscriber_on_data_available;
    dr_listener.on_subscription_matched = 
            my_typeSubscriber_on_subscription_matched;
    dr_listener.on_requested_deadline_missed = 
            my_typeSubscriber_on_requested_deadline_missed;

    // create the DataReader
    DDS_DataReader *datareader = NULL;
    datareader = DDS_Subscriber_create_datareader(
            subscriber,
            DDS_Topic_as_topicdescription(topic), 
            &dr_qos,
            &dr_listener,
            DDS_DATA_AVAILABLE_STATUS | 
            DDS_SUBSCRIPTION_MATCHED_STATUS | 
            DDS_REQUESTED_DEADLINE_MISSED_STATUS);
    if(datareader == NULL) {
        printf("ERROR: datareader == NULL\n");
    }
    
    // When we use DPSE discovery we must manually setup information about any 
    // DataWriters we are expecting to discover. This information includes a 
    // unique object ID for the remote peer (we are defining this in 
    // discovery_constants.h), as well as its Topic, type, and QoS. 
    struct DDS_PublicationBuiltinTopicData rem_publication_data =
        DDS_PublicationBuiltinTopicData_INITIALIZER;
    rem_publication_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT01_DW01;
    rem_publication_data.topic_name = DDS_String_dup(my_topic_name);
    rem_publication_data.type_name = 
            DDS_String_dup(my_typeTypePlugin_get_default_type_name());
    rem_publication_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    rem_publication_data.deadline.period.sec = 1;
    rem_publication_data.deadline.period.nanosec = 500000000;    

    // Now we can assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT01_NAME) will contain a DataWriter described by the 
    // information in the rem_publication_data struct.
    retcode = DPSE_RemotePublication_assert(
            dp,
            k_PARTICIPANT01_NAME,
            &rem_publication_data,
            my_type_get_key_kind(my_typeTypePlugin_get(), 
            NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote publication\n");
    }

    // Finally, now that all of the entities are created, we can enable them all
    retcode = DDS_Entity_enable(DDS_DomainParticipant_as_entity(dp));
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to enable entity\n");
    }

    printf("Waiting for samples to arrive, press Ctrl-C to exit\n"); 
    while(1) {
        OSAPI_Thread_sleep(10000); // sleep for 10s, then loop again
    }    
}

