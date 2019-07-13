#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/name.hpp>
#include <eosio/singleton.hpp>


#include <string>

using namespace eosio;

CONTRACT cashflowctrl : public contract {
  public:
    using contract::contract;
    
    // constructor
    cashflowctrl(eosio::name receiver, eosio::name code, datastream<const char*> ds);
    
    //destructor
    ~cashflowctrl();
    
    ACTION login(eosio::name username);
    
    // action to get account balance
    ACTION getbalance(eosio::name username);
    
    ACTION setdaylimit(eosio::name owner, eosio::asset quantity);
    
    //ACTION logout(eosio::name user);
    
   
    ACTION withdraw(eosio::name contract, eosio::name from, eosio::name to, eosio::asset quantity, std::string& memo);
    
    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(const eosio::name from, const eosio::name to, const eosio::asset quantity, std::string& memo);
    
    using transfer_action = action_wrapper<"transfer"_n, &cashflowctrl::deposit>;
    
    void transfer_to_acnt( const name& owner, const asset& amount );
    
    void transfer_from_acnt( const name& owner, const asset& amount );

    // an action that dispatches a "transaction receipt" whenever a transaction occurs
    [[eosio::action]]
    void alert(eosio::name user, std::string msg)
    {
      auto self = get_self();
      require_auth(get_self()); // authorization from this call is from the contract itself
      require_recipient(user);  // adds account to the require_recipient set and ensures that these accounts recieve notice of the executed action
    }
    
    
  
  private:
  
    void add_balance(eosio::name contract, eosio::name owner, eosio::asset value, eosio::name ram_payer);
    void sub_balance(eosio::name contract, eosio::name owner, eosio::asset value);
    
    struct state_t
    {
      eosio::name payee;
      eosio::asset limit = eosio::asset(0, symbol("TOKEN", 4));
      eosio::time_point_sec start;
      eosio::time_point_sec finish;
      
      std::string toString()
      {
        std::string info = " PAYEE " + this->payee.to_string() +
                           " LIMIT " + std::to_string(this->limit.amount) +
                           " START " + std::to_string(this->start.utc_seconds) +
                           " FINISH " + std::to_string(this->finish.utc_seconds);
        return info;
      }
    };
    
    
    // the state of the application is persisted in a singleton
    // only one instance will be stored in the RAM 
    eosio::singleton<"state"_n, state_t> state_pattern;
    
    // this will hold the present state of the application
    state_t state;
    
    
    // this is a utility function
    // it returns the state data fields  back to their default values
    state_t default_parameters() const
    {
       state_t defval;
       defval.limit.symbol = eosio::symbol("TOKEN", 4);
       defval.limit.amount = 0;
       defval.start = eosio::time_point_sec(0);
       defval.finish = eosio::time_point_sec(0);
       return defval;
    };
    
    
    TABLE account {
      name      contract;
      asset     balance;
      
      uint64_t primary_key()const { return contract.value; }
    };

    typedef multi_index< "accounts"_n, account> accounts;


    // this is a helper function, inside this an inline action will be constructed and sent
    void action_receipt(eosio::name user, std::string message)
    {
      action(
          permission_level{get_self(), "active"_n},                // permission level 
          user,                                                    // code
          "alert"_n,                                               // action 
          std::make_tuple(user, name{user}.to_string() + message)  // data 
        ).send();
    }
};
