    #ifndef NIC_CONFIG_H
    #define NIC_CONFIG_H

    // user-configurable values
    int domain_id = 100;
    char *peer = "127.0.0.1";
    
    // char *loopback_name = "lo";         // Ubuntu 20.04
    // char *eth_nic_name = "wlp0s20f3";   // Ubuntu 20.04    
    
    // char *loopback_name = "Loopback Pseudo-Interface 1";    // Windows 10
    // char *eth_nic_name = "Wireless LAN adapter Wi-Fi";      // Windows 10
    
    char *loopback_name = "lo0";        // MacOS 10.15.x
    char *eth_nic_name = "en0";         // MacOS 10.15.x 
    


    #endif