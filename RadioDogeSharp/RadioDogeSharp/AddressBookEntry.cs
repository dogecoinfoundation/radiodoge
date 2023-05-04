using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RadioDoge
{
    internal class AddressBookEntry
    {
        private const int PIN_LENGTH = 4;
        private NodeAddress nodeAddress;
        private byte[] pin;
        // Eventually we may want to include an address nickname
        //private string name;

        public AddressBookEntry(NodeAddress nodeAddress, byte[] pin)
        {
            this.nodeAddress = nodeAddress;
            this.pin = pin;
        }

        public bool ResetPin(byte[] oldPin, byte[] newPin)
        {
            if (newPin.Length != PIN_LENGTH)
            {
                return false;
            }

            for (int i = 0; i < PIN_LENGTH; i++)
            {
                if (oldPin[i] != pin[i])
                {
                    return false;
                }
            }

            pin = newPin;
            return true;
        }

        public bool DoesPinMatch(byte[] testPin)
        {
            for (int i = 0; i < PIN_LENGTH; i++)
            {
                if (testPin[i] != pin[i])
                {
                    return false;
                }
            }

            return true;
        }
    }
}
