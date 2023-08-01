using System.Runtime.InteropServices;
using System.Text;

namespace RadioDoge
{
    /// <summary>
    /// Tool for testing command and control over serial communication for Heltec LoRa modules (v2 and v3)
    /// </summary>
    public partial class SerDogeSharp
    {
        private NodeAddress localAddress = new NodeAddress(10, 0, 3);
        private NodeAddress destinationAddress = new NodeAddress(10, 0, 1);
        private NodeAddress broadcastAddress = new NodeAddress(255, 255, 255);
        private bool isLinuxOS = false;
        private const bool DEMO_MODE = true;
        private const bool TEST_LIBDOGECOIN_ON_STARTUP = false;
        private SerialPortManager portManager;

        public void Execute()
        {
            // Check operating system
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                isLinuxOS = true;
            }

            ConsoleHelper.PrintTitleScreen();

            if (TEST_LIBDOGECOIN_ON_STARTUP)
            {
                LibdogecoinFunctionalityTesting();
            }

            // Set up the serial port manager
            
            portManager = new SerialPortManager();
            portManager.RegisterDogeProcessor(ProcessDogePayload);
            if (portManager.SetupSerialConnection())
            {
                if (DEMO_MODE)
                {
                    DemoNodeSetup();
                }
                ModeSelectionLoop();
                portManager.ClosePort();
            }
        }

        /// <summary>
        /// Main loop used for allowing the user to switch between the program's functionality modes
        /// </summary>
        private void ModeSelectionLoop()
        {
            while(true)
            {
                ModeType mode = ConsoleHelper.GetUserModeSelection();
                switch (mode)
                {
                    case ModeType.LoRaSetup:
                        EnterMode(PrintSerialSetupCommandHelp, SendSetupCommand);
                        break;
                    case ModeType.SPV:
                        EnterMode(PrintSPVModeHelp, ProcessSPVCommand);
                        break;
                    case ModeType.Test:
                        EnterMode(PrintTestCommandHelp, SendTestCommand);
                        break;
                    case ModeType.Quit:
                        Console.WriteLine("Quitting the program!");
                        return;
                    default:
                        Console.WriteLine("Unknown mode selection!");
                        break;
                }
            }
        }

        /// <summary>
        /// Get user input to set the local node address (for the connected radio hardware) and the destination node address
        /// </summary>
        /// <returns></returns>
        private NodeAddress GetUserSetAddress()
        {
            while (true)
            {
                ConsoleHelper.WriteEmphasizedLine("Enter Desired Address (Format = Region.Community.Node):", ConsoleColor.Green);
                string fullAddress = Console.ReadLine();
                string[] splitAddress = fullAddress.Split('.');
                bool parseSuccess = false;
                byte region = 0;
                byte community = 0;
                byte node = 0;

                if (splitAddress.Length == 3)
                {
                    parseSuccess = Byte.TryParse(splitAddress[0], out region);
                    parseSuccess &= Byte.TryParse(splitAddress[1], out community);
                    parseSuccess &= Byte.TryParse(splitAddress[2], out node);
                }

                if (parseSuccess)
                {
                    Console.WriteLine();
                    return new NodeAddress(region, community, node);
                }
                else
                {
                    ConsoleHelper.WriteEmphasizedLine("Unable to successfully parse address!", ConsoleColor.Red);
                }
            }
        }

        private void EnterMode(Action helpFunction, Action<int> commandFunc)
        {
            bool keepGoing = true;
            while (keepGoing)
            {
                helpFunction();
                ConsoleHelper.WriteEmphasizedLine("===================\n|||Enter Command|||\n===================", ConsoleColor.Yellow);
                string message = Console.ReadLine();
                bool parseSuccess = Int32.TryParse(message, out int commandType);

                if (String.Equals("exit", message, StringComparison.InvariantCultureIgnoreCase))
                {
                    keepGoing = false;
                }
                else if (String.Equals("help", message, StringComparison.InvariantCultureIgnoreCase))
                {
                    helpFunction();
                }
                else if (parseSuccess)
                {
                    commandFunc(commandType);
                }
                // Try parsing as a single character
                else if (message.Length == 1)
                {
                    commandFunc(message[0]);
                }

                // Allow time for device to respond to sent command
                // Delay in firmware would need to be reduced to further reduce this delay
                Thread.Sleep(2000);
            }
        }
    }
}
