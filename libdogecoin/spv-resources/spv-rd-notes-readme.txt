Ok, the files that need to exist alongside the dogecoin.dll are:

-dogecoin.dll
-main_headers.db
-main_wallet.db
-spvnode.exe

The other utils, tests.exe; such.exe and sendtx.exe are just useful utilities to have.

So the FIRST TIME running spvnode; the addresses need to be registered by command line.
In the future this will happen through libdogecoin.

The FIRST RUN syntax is: "spvnode.exe -c -a "ADDR1 ADDR2 ADDR3" -b -p scan" when accessing the dll.

Our test addresses are:

D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY
DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR
DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y

I have made a startspv-rd-firstrun.bat that does this though, and should take care of it.


I have the keys in a separate file for when we start making transactions.
Each should have 1000 dogecoin in them.  

So on the hub machine, which has internet connectivity, alongside the dlls and main database run startspv-rd-firstrun.bat.
(which is just "spvnode.exe -c -a "D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y" -b -p scan" .)

It will have to synchronize (download the blockchain headers). But the included main_headers.db file should contain all headers up to a very recent syncronization so it should only take a bit.
It will also create a main_wallet.db which contains our registered watch addresses (those three) - which will hold those registered watch addresses.

(For future runs, like if you kill the process,  "spvnode.exe -c scan" alone should be sufficient.)

Each time it will have to synchronize a bit more due to passed time, but not much.

(It'll just stop scrolling and show the last block from the current date/time when it's done syncrhonizing, and it'll get new blocks as they come by around each minute.)

To exit, if need be, just ctrl-c twice for now, later on they're going to add hitting Q or escape to shut down more gracefully. 

Again to rerun just do "spvnode.exe -c scan." I made a spv-restart.bat file for that, too.
(It will show the addresses we're watching and their balances and inputs.)
Addresses watched AND utxos/balances are cached in main_wallet.db so they only need to get updated when a relevant transaction comes by. So cached responses will happen if the network goes out.

** Honestly it would probably be okay if, when our C# software is running in "i'm a hub" mode, if it just shelled out and ran/started
** "spvnode.exe -c -a "ADDR1 ADDR2 ADDR3" -b -p scan" on its own... In other words running the register command (-a with addresses) will not hurt anything if its run every time;
** and the C# could take care of the startup of it and include a list of addresses we're watching. 

-- KNOWN BUG: Right now though if we register a new address, it won't rewind the DB and 'rescan' though, so some transactions may be missed if the address had transactions in the db that were before the time we registered it. This is on the list to be fixed. A new registration should always rescan the database on disk; also a wallet corruption should trigger that, too, but it doesn't yet. For now, the transaction has to 'go by' on the network after the registration occurs for it to be picked up. But again this is a known bug.

...Once it's done synchronizing you should be able to access dogecoin.dll in c# and retrieve balances and unspent inputs with the new calls. 

(Note - New blocks should appear every one minute. Sometimes two will come in at once after two minutes though. Just depends on network conditions. If they aren't, just bang 'esc' in the console window a few times as sometimes the text display locks up and needs to be hit.)


The new calls are: 

LIBDOGECOIN_API int dogecoin_register_watch_address_with_node(char* address);
LIBDOGECOIN_API int dogecoin_unregister_watch_address_with_node(char* address);
LIBDOGECOIN_API int dogecoin_get_utxo_vector(char* address, vector* utxos);
LIBDOGECOIN_API uint8_t* dogecoin_get_utxos(char* address);
LIBDOGECOIN_API unsigned int dogecoin_get_utxos_length(char* address);
LIBDOGECOIN_API char* dogecoin_get_utxo_txid_str(char* address, unsigned int index);
LIBDOGECOIN_API uint8_t* dogecoin_get_utxo_txid(char* address, unsigned int index);
LIBDOGECOIN_API uint64_t dogecoin_get_balance(char* address);
LIBDOGECOIN_API char* dogecoin_get_balance_str(char* address);

So the first ones to try are: 

LIBDOGECOIN_API uint64_t dogecoin_get_balance(char* address);
LIBDOGECOIN_API char* dogecoin_get_balance_str(char* address);

which should return either a string or integer of the address's balance.  

Later on to form transactions from inputs that are unspent on an address we'd need to get (and store)

LIBDOGECOIN_API int dogecoin_get_utxo_vector(char* address, vector* utxos);
LIBDOGECOIN_API uint8_t* dogecoin_get_utxos(char* address);

which will list the indexes and transaction IDs for each unspent input in an address.  (We need to feed this to libdogecoin, along with the target address and private key, to create a new transaction.)

But for now the goal would be retrieving the balances via LoRa. 

There are PROBABLY some bugs in this still like if the database gets hosed or the wallet gets messed up. So I'm including a main_wallet.bak in there, and a database checkpoint from April. 
If for some reason it gets messed up just cp the main_wallet.bak to main_wallet.db and main_database_april.bak to main_database.db and it'll sync from some time in April and should restore everything fine.









