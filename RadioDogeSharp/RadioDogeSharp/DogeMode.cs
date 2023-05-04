using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private Dictionary<string, AddressBookEntry> dogeAddressBook = new Dictionary<string, AddressBookEntry>();
        private const int PIN_LENGTH = 4;
        private const int MAX_ADDRESS_LENGTH = 35;
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

        private void ProcessDogePayload(NodeAddress senderAddress, byte[] payload)
        {
            DogeCommandType commType = (DogeCommandType)payload[0];
            switch (commType)
            {
                case DogeCommandType.SendDogeAddress:
                    // This means we got sent a dogecoin address
                    int addrLength = payload.Length - 1;
                    if (addrLength <= MAX_ADDRESS_LENGTH)
                    {
                        string addressString = ExtractDogecoinAddressFromPayload(1, addrLength, payload);
                        Console.WriteLine($"Received Dogecoin Address: {addressString}");
                    }
                    else
                    {
                        Console.WriteLine("Received an address that was too long!");
                    }
                    break;
                case DogeCommandType.GetDogeAddress:
                    // This means someone is requesting a dogecoin address so we would send ours out
                    SendDogeAddress(senderAddress);
                    break;
                case DogeCommandType.RegisterAddress:
                    RegisterDogeAddress(senderAddress, payload);
                    break;
                default:
                    Console.WriteLine("Unknown payload. Raw data:");
                    PrintPayloadAsHex(payload);
                    break;
            }
        }

        private string ExtractDogecoinAddressFromPayload(int offsetIndex, int addrLength, byte[] payload)
        {
            char[] dogecoinAddress = new char[addrLength];
            Array.Copy(payload, offsetIndex, dogecoinAddress, 0, addrLength);
            return new string(dogecoinAddress);
        }

        private void RegisterDogeAddress(NodeAddress sender, byte[] payload)
        {
            // We are expecting an address and a pin here which we will add to our dictionary
            int addressLength = payload.Length - (PIN_LENGTH + 1);
            if (addressLength <= MAX_ADDRESS_LENGTH)
            {
                string addressString = ExtractDogecoinAddressFromPayload(1, addressLength, payload);
                // Extract pin from payload
                byte[] extractedPin = new byte[PIN_LENGTH];
                Array.Copy(payload, addressLength + 1, extractedPin, 0, PIN_LENGTH);
                Console.WriteLine($"Registering Dogecoin Address: {addressString}");
                string pinString = "";
                for (int i = 0; i < PIN_LENGTH; i++)
                {
                    pinString += extractedPin[i].ToString();
                }

                Console.WriteLine($"Pin: {pinString}");
                bool alreadyRegistered = dogeAddressBook.ContainsKey(addressString);
                if (!alreadyRegistered)
                {
                    AddressBookEntry entry = new AddressBookEntry(sender, extractedPin);
                    dogeAddressBook.Add(addressString, entry);
                }
                else
                {
                    Console.WriteLine("Address already registered!");
                }
            }
            else
            {
                Console.WriteLine("Failed to register dogecoin address!");
            }
        }

        private void SendDogeAddress(NodeAddress destNode)
        {
            // Generate a test address for now
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            LibDogecoin.dogecoin_ecc_start();
            int successret = LibDogecoin.generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successret == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();
                Console.WriteLine($"Public key: {publickey}");

                byte[] dogeCoinAddress = Encoding.ASCII.GetBytes(publickey);
                List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
                payload.Add((byte)DogeCommandType.SendDogeAddress);
                payload.AddRange(dogeCoinAddress);
                SendPacket(destNode, payload.ToArray());
            }

            LibDogecoin.dogecoin_ecc_stop();
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
