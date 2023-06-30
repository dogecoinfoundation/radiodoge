using System.IO.Ports;

namespace RadioDoge
{
    internal class SerialPortManager
    {
        private SerialPort port;
        private const string DEFAULT_PORT = "COM3";
        private bool retryConnect = true;
        private MultipartPacket currMultipartPacket = new MultipartPacket();
        private Action<NodeAddress, byte[]> processDogePayloadFunction;

        public void RegisterDogeProcessor(Action<NodeAddress, byte[]> processDogePayloadFunction)
        {
            this.processDogePayloadFunction = processDogePayloadFunction;
        }

        public bool SetupSerialConnection()
        {
            Console.WriteLine("Setting up connection to LoRa module...");
            if (OpenPortHelper(DEFAULT_PORT))
            {
                ConsoleHelper.WriteEmphasizedLine("WOW SO CONNECTED! Connection to LoRa module successful!", ConsoleColor.Green);
                return true;
            }
            else
            {
                Console.WriteLine("Enter 'quit' to exit the program or anything else to try connecting again!");
                string userInput = Console.ReadLine();
                if (!userInput.Equals("quit", StringComparison.InvariantCultureIgnoreCase))
                {
                    return SetupSerialConnection();
                }
            }
            return false;
        }

        public void ClosePort()
        {
            if (port != null)
            {
                port.Close();
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
                ConsoleHelper.WriteEmphasizedLine($"Failed to open up {portToOpen}", ConsoleColor.Red);
                string[] ports = SerialPort.GetPortNames();
                if (ports.Length > 0)
                {
                    Console.WriteLine("The following serial ports were found:");
                    // Display each port name to the console.
                    int portIndex = 0;
                    foreach (string port in ports)
                    {
                        Console.WriteLine($"{portIndex}: {port}");
                        portIndex++;
                    }

                    // Try automatically connecting if there is only one available com port
                    if (ports.Length == 1 && retryConnect)
                    {
                        Console.WriteLine($"Trying again with port {ports[0]}");
                        retryConnect = false;
                        return OpenPortHelper(ports[0]);
                    }
                    else if (ports.Length > 1)
                    {
                        // Ask user to specify which port they would like to open
                        while (true)
                        {
                            Console.WriteLine($"Enter a port index: 0-{ports.Length - 1}");
                            bool parseSuccess = int.TryParse(Console.ReadLine(), out int selectedIndex);
                            if (parseSuccess && selectedIndex >= 0 && selectedIndex < ports.Length)
                            {
                                Console.WriteLine($"Trying again with port {ports[selectedIndex]}");
                                retryConnect = false;
                                return OpenPortHelper(ports[selectedIndex]);
                            }
                            else
                            {
                                Console.WriteLine($"Invalid selection!");
                            }
                        }
                    }
                }
                else
                {
                    Console.WriteLine("No serial devices found!");
                }

                return false;
            }
        }

        public void WriteToPort(byte[] buffer, int offset, int count)
        {
            port.Write(buffer, offset, count);
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
        /// Send a single packet containing the provided payload to the specified destination address
        /// </summary>
        /// <param name="destAddress"></param>
        /// <param name="payload"></param>
        public void SendPacket(NodeAddress localAddress, NodeAddress destAddress, byte[] payload)
        {
            byte[] commandToSend = PacketHelper.CreatePacket(destAddress, localAddress, payload);
            ConsoleHelper.PrintCommandBytes(commandToSend);
            WriteToPort(commandToSend, 0, commandToSend.Length);
        }

        /// <summary>
        /// Send a multipart packet containing the provided payload (broken up into multiple parts) to the specified destination node
        /// </summary>
        /// <param name="destAddress"></param>
        /// <param name="multipartPayload"></param>
        public void SendMultipartPacket(NodeAddress localAddress, NodeAddress destAddress, byte[] multipartPayload)
        {
            // Create all the packet parts
            byte[][] allPacketParts = PacketHelper.CreateMultipartPackets(destAddress, localAddress, multipartPayload);
            // Send out the parts one by one
            for (int i = 0; i < allPacketParts.Length; i++)
            {
                ConsoleHelper.PrintCommandBytes(allPacketParts[i]);
                WriteToPort(allPacketParts[i], 0, allPacketParts[i].Length);
                // Delay a bit between the sending of each piece
                Thread.Sleep(1000);
            }
        }

        /// <summary>
        /// Callback to process/collect data when something is received on the serial port
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void PortDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            // Show all the incoming data in the port's buffer
            Thread.Sleep(500);
            SerialCommandType commandType = (SerialCommandType)port.ReadByte();
            int payloadSize = port.ReadByte();
            byte[] payload = new byte[payloadSize];
            while (port.BytesToRead < payloadSize)
            {
                Thread.Sleep(50);
            }
            port.Read(payload, 0, payloadSize);
            if (commandType == SerialCommandType.MultipartPacket)
            {
                PacketReconstructionCode result = currMultipartPacket.AddPacketPiece(payload);
                if (result == PacketReconstructionCode.Complete)
                {
                    Console.WriteLine(currMultipartPacket.ToString());
                    byte[] completePayload = currMultipartPacket.GetPayload();
                    ConsoleHelper.PrintPayloadAsHex(completePayload);
                    processDogePayloadFunction(currMultipartPacket.senderAddress, completePayload);
                    // @TODO do something useful with the payload
                    currMultipartPacket = new MultipartPacket();
                }
            }
            else if (commandType == SerialCommandType.HostFormedPacket)
            {
                Console.WriteLine($"Command: {commandType}, Payload Size: {payloadSize}");
                byte[] dataPayload = ExtractHostFormedPacketData(payload, out NodeAddress currSenderAddr);
                processDogePayloadFunction(currSenderAddr, dataPayload);
            }
            else
            {
                Console.WriteLine($"Command: {commandType}, Payload Size: {payloadSize}");
                ConsoleHelper.PrintPayloadAsHex(payload);
                ProcessSerialSetupCommandPayload(commandType, payload);
            }
        }

        private void ProcessSerialSetupCommandPayload(SerialCommandType commandType, byte[] payload)
        {
            switch (commandType)
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
