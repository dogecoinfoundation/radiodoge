﻿namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private byte[] testAddress = { 0x44 };

        private void SendDogeCommand(int commandValue)
        {
            DogeCommandType dogeCommandType = (DogeCommandType)commandValue;
            List<byte> commandBytes = new List<byte>();
            ConsoleHelper.WriteEmphasizedLine($"Sending Command: {dogeCommandType}", ConsoleColor.Yellow);
            switch (dogeCommandType)
            {
                case DogeCommandType.GetDogeAddress:
                    RequestDogeCoinAddress(destinationAddress);
                    return;
                case DogeCommandType.GetBalance:
                    RequestDogeCoinBalance(destinationAddress, testAddress);
                    break;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
            byte[] commandToSend = commandBytes.ToArray();
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void RequestDogeCoinAddress(NodeAddress destNode)
        {
            // Create payload
            byte[] payload = new byte[] { (byte)DogeCommandType.GetDogeAddress };
            SendPacket(destNode, payload);
        }

        private void RequestDogeCoinBalance(NodeAddress destNode, byte[] dogeCoinAddress)
        {
            List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
            payload.Add((byte)DogeCommandType.GetBalance);
            payload.AddRange(dogeCoinAddress);
            SendPacket(destNode, payload.ToArray());
        }

        private void ProcessDogePayload(byte[] payload)
        {
            DogeCommandType commType = (DogeCommandType)payload[0];
            switch (commType)
            {
                case DogeCommandType.SendDogeAddress:
                    // This means we got sent a dogecoin address
                    int addrLength = payload.Length - 1;
                    char[] dogecoinAddress = new char[addrLength];
                    Array.Copy(payload, 1, dogecoinAddress, 0, addrLength);
                    string addressString = new string(dogecoinAddress);
                    Console.WriteLine($"Received Dogecoin Address: {addressString}");
                    break;
                case DogeCommandType.GetDogeAddress:
                    // This means someone is requesting a dogecoin address so we would send ours out
                    // @TODO
                    break;
                default:
                    Console.WriteLine("Unknown payload. Raw data:");
                    PrintPayloadAsHex(payload);
                    break;
            }
        }

        private void PrintDogeCommandHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available Doge Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(DogeCommandType)))
            {
                ConsoleHelper.WriteEmphasizedLine($"{i}: {(DogeCommandType)i}", ConsoleColor.Cyan);
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }
    }
}