using System.IO.Ports;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private SerialPort port;
        private const string DEFAULT_PORT = "COM3";
        private bool retryConnect = true;
        private MultipartPacket currMultipartPacket = new MultipartPacket();

        private bool SetupSerialConnection()
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

        private void ClosePort()
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
                    ProcessDogePayload(currMultipartPacket.senderAddress, completePayload);
                    // @TODO do something useful with the payload
                    currMultipartPacket = new MultipartPacket();
                }
            }
            else if (commandType == SerialCommandType.HostFormedPacket)
            {
                Console.WriteLine($"Command: {commandType}, Payload Size: {payloadSize}");
                byte[] dataPayload = ExtractHostFormedPacketData(payload, out NodeAddress currSenderAddr);
                ProcessDogePayload(currSenderAddr, dataPayload);
            }
            else
            {
                Console.WriteLine($"Command: {commandType}, Payload Size: {payloadSize}");
                ConsoleHelper.PrintPayloadAsHex(payload);
                ProcessSerialSetupCommandPayload(commandType, payload);
            }
        }
    }
}
