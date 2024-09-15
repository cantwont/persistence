# Persistence in C

This C program installs a customizable Windows service with high-level privileges under the `LocalSystem` account. The service can be configured with any name and description as needed. It continuously runs in the background and is designed to simulate critical system functionality.

Key Features:

-   **Customizable Service Name & Description:** You can easily modify the name and description of the service to suit your use case.
-   **Automatic System Crash on Stop Attempt:** If the service receives a stop or restart command, it triggers a Blue Screen of Death (BSOD), preventing it from being stopped by regular means.
-   **Service Auto-Start:** The service starts automatically when the system boots and runs with system-level privileges.

The only possible way to delete this service is by first turning off Automatic Start-Up in the Services app, restarting the machine, and then deleting wherever the executable is stored.

https://github.com/user-attachments/assets/4d3fefaa-d941-4260-87f8-98c1b89142ae
