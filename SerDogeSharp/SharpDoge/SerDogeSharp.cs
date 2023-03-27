using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Text;
using System.Threading;

namespace RadioDoge
{
    /// <summary>
    /// Tool for testing command and control over serial communication for Heltec LoRa modules (v2 and v3)
    /// </summary>
    public class SerDogeSharp
    {
        private SerialPort port;
        private int selectedPort = 3;
        private NodeAddress localAddress = new NodeAddress(10, 0, 3);
        private NodeAddress destinationAddress = new NodeAddress(10, 0, 1);
        private readonly byte terminator = 255;
        private const int MAX_PAYLOAD_BYTES = 192;
        private bool retryConnect = true;

        class NodeAddress
        {
            public byte region, community, node;

            public NodeAddress(byte region, byte community, byte node)
            {
                this.region = region;
                this.community = community;
                this.node = node;
            }

            public override string ToString()
            {
                return $"{region}.{community}.{node}";
            }

            public byte[] ToByteArray()
            {
                return new byte[] { region, community, node };
            }
        }

        private enum SerialCommandType
        {
            None,
            GetAddress,
            SetAddresses,
            Ping,
            Message,
            HardwareInfo = 0x3f, //Translates to sending '?'
            HostFormedPacket = 0x68, // Translates to sending 'h'
            MultipartPacket = 0x6D, // Translates to sending 'm'
        }

        public void Execute()
        {
            string portToOpen = $"COM{selectedPort}";
            if (OpenPortHelper(portToOpen))
            {
                SerialCommandLoop();
                port.Close();
            }
        }

        private void SendCommand(SerialCommandType commandType)
        {
            List<byte> commandBytes = new List<byte>();
            ConsoleWriteEmphasizedLine($"Sending Command: {commandType}", ConsoleColor.Yellow);
            switch (commandType)
            {
                case SerialCommandType.GetAddress:
                    commandBytes.AddRange(CreateCommandHeader((byte)commandType, 0));
                    break;
                case SerialCommandType.SetAddresses:
                    ConsoleWriteEmphasizedLine("Please set the local address...", ConsoleColor.Cyan);
                    localAddress = GetUserSetAddress();
                    ConsoleWriteEmphasizedLine("Please set the destination address...", ConsoleColor.Cyan);
                    destinationAddress = GetUserSetAddress();
                    byte[] setHeader = CreateCommandHeader((byte)commandType, 3);
                    commandBytes.AddRange(setHeader);
                    commandBytes.AddRange(localAddress.ToByteArray());
                    break;
                case SerialCommandType.Ping:
                    byte[] pingHeader = CreateCommandHeader((byte)commandType, 3);
                    commandBytes.AddRange(pingHeader);
                    commandBytes.AddRange(destinationAddress.ToByteArray());
                    break;
                case SerialCommandType.Message:
                    Console.WriteLine("Enter message:");
                    string messageToSend = Console.ReadLine();
                    byte[] messageBytes = Encoding.ASCII.GetBytes(messageToSend);
                    byte payloadSize = (byte)(messageBytes.Length + 7);
                    commandBytes.AddRange(CreateCommandHeader((byte)commandType, payloadSize));
                    commandBytes.Add((byte)SerialCommandType.Message);
                    commandBytes.AddRange(localAddress.ToByteArray());
                    commandBytes.AddRange(destinationAddress.ToByteArray());
                    commandBytes.AddRange(messageBytes);
                    break;
                case SerialCommandType.HardwareInfo:
                    commandBytes.AddRange(CreateCommandHeader((byte)commandType, 0));
                    break;
                case SerialCommandType.HostFormedPacket:
                    ConsoleWriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMessage = Console.ReadLine();
                    byte[] convertedMessageBytes = Encoding.ASCII.GetBytes(readMessage);
                    byte messageSize = (byte)(convertedMessageBytes.Length + 6);
                    commandBytes.AddRange(CreateCommandHeader((byte)commandType, (byte)(messageSize + 2)));
                    commandBytes.AddRange(CreateCommandHeader((byte)commandType, messageSize));
                    commandBytes.AddRange(localAddress.ToByteArray());
                    commandBytes.AddRange(destinationAddress.ToByteArray());
                    commandBytes.AddRange(convertedMessageBytes);
                    break;
                case SerialCommandType.MultipartPacket:
                    SendMultipartMessage();
                    // No need to continue as it is all handled in the SendMultipartMessage function so we will just return
                    return;
                default:
                    ConsoleWriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
            byte[] commandToSend = commandBytes.ToArray();
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void SendMultipartMessage()
        {
            byte commandType = (byte)SerialCommandType.MultipartPacket;
            /*
            ConsoleHelper.ConsoleWriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
            string readMultipartMessage = Console.ReadLine();
            byte[] convertedMultipartMessageBytes = Encoding.ASCII.GetBytes(readMultipartMessage);
            */
            byte[] convertedMultipartMessageBytes = new byte[1024];
            for (int i = 0; i < convertedMultipartMessageBytes.Length; i++)
            {
                convertedMultipartMessageBytes[i] = (byte)(i % 256);
            }
            // Determine how many packets we are going to create
            int totalNumPackets = convertedMultipartMessageBytes.Length / MAX_PAYLOAD_BYTES;
            if (convertedMultipartMessageBytes.Length % MAX_PAYLOAD_BYTES != 0)
            {
                totalNumPackets++;
            }
            List<byte> currPacketBytes = new List<byte>();
            int payloadLeft = convertedMultipartMessageBytes.Length;
            int payloadInd = 0;
            int currPayloadLength = 0;
            for (int i = 1; i < totalNumPackets + 1; i++)
            {
                if (payloadLeft >= MAX_PAYLOAD_BYTES)
                {
                    currPayloadLength = MAX_PAYLOAD_BYTES;
                }
                else
                {
                    currPayloadLength = payloadLeft;
                }

                // Add in the command header
                byte[] serialHeader = CreateCommandHeader(commandType, (byte)(currPayloadLength + 12));
                currPacketBytes.AddRange(serialHeader);
                byte[] packetHeader = CreateCommandHeader(commandType, (byte)(currPayloadLength + 10));
                currPacketBytes.AddRange(packetHeader);

                // Add in address information
                currPacketBytes.AddRange(localAddress.ToByteArray());
                currPacketBytes.AddRange(destinationAddress.ToByteArray());

                // Add in multipacket count information
                currPacketBytes.Add(123); // default msg id for now
                currPacketBytes.Add(0); // reserved
                currPacketBytes.Add((byte)i);
                currPacketBytes.Add((byte)totalNumPackets);

                // Add in payload now
                byte[] currPayload = new byte[currPayloadLength];
                Array.Copy(convertedMultipartMessageBytes, payloadInd, currPayload, 0, currPayloadLength);
                payloadInd += currPayloadLength;
                payloadLeft -= currPayloadLength;
                currPacketBytes.AddRange(currPayload);

                // Send out the packet now
                byte[] commandToSend = currPacketBytes.ToArray();
                PrintCommandBytes(commandToSend);
                port.Write(commandToSend, 0, commandToSend.Length);
                currPacketBytes.Clear();
                Thread.Sleep(1000);
            }
        }

        private byte[] CreateCommandHeader(byte commandType, byte payloadSize)
        {
            return new byte[]
            {
                commandType, payloadSize
            };
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
                ConsoleWriteEmphasizedLine("Enter Desired Address (Format = Region.Community.Node):", ConsoleColor.Green);
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
                    ConsoleWriteEmphasizedLine("Unable to successfully parse address!", ConsoleColor.Red);
                }
            }
        }

        private bool OpenPortHelper(string portToOpen)
        {
            Console.WriteLine($"Attempting to open {portToOpen}");
            try
            {
                // Create the serial port with basic settings
                port = new SerialPort(portToOpen, 115200, Parity.None, 8, StopBits.One);
                // Attach a method to be called when there is data waiting in the port's buffer
                port.DataReceived += new SerialDataReceivedEventHandler(PortDataReceived);
                // Begin communications
                port.Open();
                return true;
            }
            catch
            {
                ConsoleWriteEmphasizedLine($"Failed to open up {portToOpen}", ConsoleColor.Red);
                string[] ports = SerialPort.GetPortNames();
                if (ports.Length > 0)
                {
                    Console.WriteLine("The following serial ports were found:");
                    // Display each port name to the console.
                    foreach (string port in ports)
                    {
                        Console.WriteLine(port);
                    }

                    // Try automatically connecting if there is only one available com port
                    if (ports.Length == 1 && retryConnect)
                    {
                        Console.WriteLine($"Trying again with port {ports[0]}");
                        retryConnect = false;
                        return OpenPortHelper(ports[0]);
                    }
                }
                else
                {
                    Console.WriteLine("No serial devices found!");
                }

                return false;
            }
        }

        private void SerialCommandLoop()
        {
            PrintCommandHelp();

            bool keepGoing = true;
            while (keepGoing)
            {
                ConsoleWriteEmphasizedLine("===================\n|||Enter Command|||\n===================", ConsoleColor.Yellow);
                string message = Console.ReadLine();
                bool parseSuccess = Int32.TryParse(message, out int commandType);

                if (String.Equals("quit", message))
                {
                    keepGoing = false;
                }
                else if (String.Equals("help", message))
                {
                    PrintCommandHelp();
                }
                else if (parseSuccess)
                {
                    SendCommand((SerialCommandType)commandType);
                }
                // Try parsing as a single character
                else if (message.Length == 1)
                {
                    SendCommand((SerialCommandType)message[0]);
                }
            }
        }

        private void PortDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            // Show all the incoming data in the port's buffer
            Thread.Sleep(500);
            int commandType = port.ReadByte();
            int payloadSize = port.ReadByte();
            byte[] payload = new byte[payloadSize];
            while (port.BytesToRead < payloadSize)
            {
                Thread.Sleep(50);
            }
            port.Read(payload, 0, payloadSize);
            Console.WriteLine($"Command: {commandType}, Payload Size: {payloadSize}");
            StringBuilder hex = new StringBuilder(payload.Length * 2);
            foreach (byte b in payload)
            {
                hex.AppendFormat("{0:x2}", b);
            }
            Console.WriteLine(hex.ToString());
        }

        private void PrintCommandHelp()
        {
            ConsoleWriteEmphasizedLine("Available Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(SerialCommandType)))
            {
                if (i < (int)'!')
                {
                    ConsoleWriteEmphasizedLine($"{i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
                else
                {
                    ConsoleWriteEmphasizedLine($"{i} or {(char)i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
            }
            ConsoleWriteEmphasizedLine("Enter 'quit' to exit or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        private void ConsoleWriteEmphasizedLine(string text, ConsoleColor consoleColor)
        {
            Console.ForegroundColor = consoleColor;
            Console.WriteLine(text);
            Console.ForegroundColor = ConsoleColor.White;
        }
    }
}
