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

        public void Execute()
        {
            ConsoleHelper.PrintTitleScreen();
            if (SetupSerialConnection())
            {
                ModeSelectionLoop();
                ClosePort();
            }
        }

        private void ModeSelectionLoop()
        {
            while(true)
            {
                ModeSelection mode = ConsoleHelper.GetUserModeSelection();
                switch (mode)
                {
                    case ModeSelection.LoRaSetup:
                        SerialSetupCommandLoop();
                        break;
                    case ModeSelection.Doge:
                        DogeCommandLoop();
                        break;
                    case ModeSelection.Test:
                        // @TODO
                        ConsoleHelper.WriteEmphasizedLine("Test mode unavailable!", ConsoleColor.Red);
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

        private void SendSerialCommand(SerialCommandType commandType)
        {
            List<byte> commandBytes = new List<byte>();
            ConsoleHelper.WriteEmphasizedLine($"Sending Command: {commandType}", ConsoleColor.Yellow);
            switch (commandType)
            {
                case SerialCommandType.GetNodeAddress:
                    commandBytes.AddRange(PacketHelper.CreateCommandHeader((byte)commandType, 0));
                    break;
                case SerialCommandType.SetNodeAddresses:
                    ConsoleHelper.WriteEmphasizedLine("Please set the local address...", ConsoleColor.Cyan);
                    localAddress = GetUserSetAddress();
                    ConsoleHelper.WriteEmphasizedLine("Please set the destination address...", ConsoleColor.Cyan);
                    destinationAddress = GetUserSetAddress();
                    byte[] setHeader = PacketHelper.CreateCommandHeader((byte)commandType, 3);
                    commandBytes.AddRange(setHeader);
                    commandBytes.AddRange(localAddress.ToByteArray());
                    break;
                case SerialCommandType.Ping:
                    byte[] pingHeader = PacketHelper.CreateCommandHeader((byte)commandType, 3);
                    commandBytes.AddRange(pingHeader);
                    commandBytes.AddRange(destinationAddress.ToByteArray());
                    break;
                case SerialCommandType.Message:
                    Console.WriteLine("Enter message:");
                    string messageToSend = Console.ReadLine();
                    byte[] messageBytes = Encoding.ASCII.GetBytes(messageToSend);
                    byte payloadSize = (byte)(messageBytes.Length + 7);
                    commandBytes.AddRange(PacketHelper.CreateCommandHeader((byte)commandType, payloadSize));
                    commandBytes.Add((byte)SerialCommandType.Message);
                    commandBytes.AddRange(localAddress.ToByteArray());
                    commandBytes.AddRange(destinationAddress.ToByteArray());
                    commandBytes.AddRange(messageBytes);
                    break;
                case SerialCommandType.HardwareInfo:
                    commandBytes.AddRange(PacketHelper.CreateCommandHeader((byte)commandType, 0));
                    break;
                case SerialCommandType.HostFormedPacket:
                    /*
                    ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMessage = Console.ReadLine();
                    byte[] convertedMessageBytes = Encoding.ASCII.GetBytes(readMessage);
                    SendPacket(destinationAddress,convertedMessageBytes);
                    */
                    RequestDogeCoinAddress(destinationAddress);
                    return;
                case SerialCommandType.MultipartPacket:
                    /*
                    ConsoleHelper.ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMultipartMessage = Console.ReadLine();
                    byte[] convertedMultipartMessageBytes = Encoding.ASCII.GetBytes(readMultipartMessage);
                    */
                    byte[] convertedMultipartMessageBytes = new byte[1024];
                    for (int i = 0; i < convertedMultipartMessageBytes.Length; i++)
                    {
                        convertedMultipartMessageBytes[i] = (byte)(i % 256);
                    }
                    SendMultipartPacket(convertedMultipartMessageBytes);
                    // No need to continue as sending the message is all handled in the SendMultipartMessage function so we will just return
                    return;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
            byte[] commandToSend = commandBytes.ToArray();
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void SendPacket(NodeAddress destAddress, byte[] payload)
        {
            byte[] commandToSend = PacketHelper.CreatePacket(destAddress, localAddress, payload);
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void SendMultipartPacket(byte[] multipartPayload)
        {
            // Create all the packet parts
            byte[][] allPacketParts = PacketHelper.CreateMultipartPackets(destinationAddress, localAddress, multipartPayload);
            // Send out the parts one by one
            for (int i = 0; i < allPacketParts.Length; i++)
            {
                PrintCommandBytes(allPacketParts[i]);
                port.Write(allPacketParts[i], 0, allPacketParts[i].Length);
                // Delay a bit between the sending of each piece
                Thread.Sleep(1000);
            }
        }

        private void PrintCommandBytes(byte[] commandToSend)
        {
            Console.Write($"Host sent {commandToSend.Length - 1} bytes: ");
            for (int i = 0; i < commandToSend.Length; i++)
            {
                Console.Write(commandToSend[i].ToString("X2") + " ");
            }
            Console.WriteLine();
        }

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

        private void SerialSetupCommandLoop()
        {
            ConsoleHelper.PrintSerialSetupCommandHelp();

            bool keepGoing = true;
            while (keepGoing)
            {
                ConsoleHelper.WriteEmphasizedLine("===================\n|||Enter Command|||\n===================", ConsoleColor.Yellow);
                string message = Console.ReadLine();
                bool parseSuccess = Int32.TryParse(message, out int commandType);

                if (String.Equals("exit", message, StringComparison.InvariantCultureIgnoreCase))
                {
                    keepGoing = false;
                }
                else if (String.Equals("help", message, StringComparison.InvariantCultureIgnoreCase))
                {
                    ConsoleHelper.PrintSerialSetupCommandHelp();
                }
                else if (parseSuccess)
                {
                    SendSerialCommand((SerialCommandType)commandType);
                }
                // Try parsing as a single character
                else if (message.Length == 1)
                {
                    SendSerialCommand((SerialCommandType)message[0]);
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

        private void PrintPayloadAsHex(byte[] payload)
        {
            StringBuilder hex = new StringBuilder(payload.Length * 2);
            foreach (byte b in payload)
            {
                hex.AppendFormat("{0:x2}", b);
            }
            Console.WriteLine(hex.ToString());
        }

        private void ProcessSerialCommandPayload(SerialCommandType commandType, byte[] payload)
        {
            switch(commandType)
            {
                case SerialCommandType.GetNodeAddress:
                    ConsoleHelper.WriteEmphasizedLine($"Local Address: {payload[0]}.{payload[1]}.{payload[2]}", ConsoleColor.Green);
                    break;
                case SerialCommandType.HardwareInfo:
                    if ((char)payload[0] == 'h')
                    {
                        ConsoleHelper.WriteEmphasizedLine($"Heltec LoRa WiFi LoRa 32 (V{payload[1]})\nFirmware version {payload[2]}", ConsoleColor.Green);
                    }
                    else
                    {
                        ConsoleHelper.WriteEmphasizedLine($"Unknown hardware!", ConsoleColor.Red);
                    }
                    break;
                case SerialCommandType.Message:
                    break;
                case SerialCommandType.ResultCode:
                    if (payload[0] == 0x06)
                    {
                        ConsoleHelper.WriteEmphasizedLine("Device ACK'd Command", ConsoleColor.Green);
                    }
                    else
                    {
                        ConsoleHelper.WriteEmphasizedLine("ERROR: Device sent a NACK!", ConsoleColor.Red);
                    }
                    break;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown payload type!", ConsoleColor.Red);
                    break;
            }
        }
    }
}
