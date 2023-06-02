using System.Data;
using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        internal static void PrintSerialSetupCommandHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available LoRa Setup Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(SerialCommandType)))
            {
                if (i < (int)'!')
                {
                    ConsoleHelper.WriteEmphasizedLine($"{i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
                // For now we don't want to have host formed packet or multipart controls in this mode
                else if (i < 0x60)
                {
                    ConsoleHelper.WriteEmphasizedLine($"{i} or {(char)i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        private void CommandSetNodeAddress(NodeAddress setAddress)
        {
            List<byte> commandBytes = new List<byte>();
            byte[] setHeader = PacketHelper.CreateCommandHeader((byte)SerialCommandType.SetNodeAddresses, 3);
            commandBytes.AddRange(setHeader);
            commandBytes.AddRange(setAddress.ToByteArray());
            byte[] commandToSend = commandBytes.ToArray();
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void SendSetupCommand(int commandValue)
        {
            SerialCommandType commandType = (SerialCommandType)commandValue;
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
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
            byte[] commandToSend = commandBytes.ToArray();
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
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
