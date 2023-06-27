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
        private MultipartPacket currMultipartPacket = new MultipartPacket();
        private bool isLinuxOS = false;
        private bool demoMode = true;

        public void Execute()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                isLinuxOS = true;
            }

            ConsoleHelper.PrintTitleScreen();   
            if (SetupSerialConnection())
            {
                if (demoMode)
                {
                    //LibdogecoinFunctionalityTesting();
                    DemoNodeSetup();
                }
                ModeSelectionLoop();
                ClosePort();
            }
        }

        /// <summary>
        /// Main loop used for allowing the user to switch between the program's functionality modes
        /// </summary>
        private void ModeSelectionLoop()
        {
            while(true)
            {
                ModeSelection mode = ConsoleHelper.GetUserModeSelection();
                switch (mode)
                {
                    case ModeSelection.LoRaSetup:
                        EnterMode(PrintSerialSetupCommandHelp, SendSetupCommand);
                        break;
                    case ModeSelection.Doge:
                        EnterMode(PrintDogeCommandHelp, SendDogeCommand);
                        break;
                    case ModeSelection.Test:
                        EnterMode(PrintTestCommandHelp, SendTestCommand);
                        break;
                    case ModeSelection.Quit:
                        Console.WriteLine("Quitting the program!");
                        return;
                    default:
                        Console.WriteLine("Unknown mode selection!");
                        break;
                }
            }
        }

        /// <summary>
        /// Send a single packet containing the provided payload to the specified destination address
        /// </summary>
        /// <param name="destAddress"></param>
        /// <param name="payload"></param>
        private void SendPacket(NodeAddress destAddress, byte[] payload)
        {
            byte[] commandToSend = PacketHelper.CreatePacket(destAddress, localAddress, payload);
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        /// <summary>
        /// Send a multipart packet containing the provided payload (broken up into multiple parts) to the specified destination node
        /// </summary>
        /// <param name="destAddress"></param>
        /// <param name="multipartPayload"></param>
        private void SendMultipartPacket(NodeAddress destAddress, byte[] multipartPayload)
        {
            // Create all the packet parts
            byte[][] allPacketParts = PacketHelper.CreateMultipartPackets(destAddress, localAddress, multipartPayload);
            // Send out the parts one by one
            for (int i = 0; i < allPacketParts.Length; i++)
            {
                PrintCommandBytes(allPacketParts[i]);
                port.Write(allPacketParts[i], 0, allPacketParts[i].Length);
                // Delay a bit between the sending of each piece
                Thread.Sleep(1000);
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

        private byte[] ExtractHostFormedPacketData(byte[] rawPayload, out NodeAddress senderAddress)
        {
            senderAddress = new NodeAddress(rawPayload[0], rawPayload[1], rawPayload[2]);
            int dataLen = rawPayload.Length - 6;
            byte[] dataPortion = new byte[dataLen];
            Array.Copy(rawPayload, 6, dataPortion, 0, dataLen);
            return dataPortion;
        }

        /// <summary>
        /// Print the received host command (byte array)
        /// </summary>
        /// <param name="commandToSend"></param>
        private void PrintCommandBytes(byte[] commandToSend)
        {
            Console.Write($"Host sent {commandToSend.Length - 1} bytes: ");
            for (int i = 0; i < commandToSend.Length; i++)
            {
                Console.Write(commandToSend[i].ToString("X2") + " ");
            }
            Console.WriteLine();
        }

        /// <summary>
        /// Print a provided payload in hexadecimal format
        /// </summary>
        /// <param name="payload"></param>
        private void PrintPayloadAsHex(byte[] payload)
        {
            StringBuilder hex = new StringBuilder(payload.Length * 2);
            foreach (byte b in payload)
            {
                hex.AppendFormat("{0:x2}", b);
            }
            Console.WriteLine(hex.ToString());
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
