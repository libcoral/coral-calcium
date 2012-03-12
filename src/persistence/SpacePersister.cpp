/*
 * Calcium - Domain Model Framework
 * See copyright notice in LICENSE.md
 */

#include "SpacePersister_Base.h"

#include <ca/ISpaceChanges.h>
#include <ca/IUniverse.h>
#include <ca/ISpace.h>
#include <ca/IOException.h>
#include <ca/FormatException.h>
#include <ca/ISpaceStore.h>

#include <co/RefVector.h>
#include <co/RefPtr.h>
#include <co/IObject.h>
#include <co/IField.h>
#include <co/IArray.h>
#include <co/Any.h>
#include <co/NotSupportedException.h>
#include <co/IllegalArgumentException.h>
#include <co/IllegalCastException.h>
#include <co/IllegalStateException.h>
#include <co/Log.h>

#include <set>
#include <map>

#include <ca/IObjectChanges.h>
#include <ca/ChangedConnection.h>
#include <ca/ChangedRefVecField.h>
#include <ca/ChangedRefField.h>
#include <ca/ChangedValueField.h>
#include <ca/IServiceChanges.h>

#include <lua/IState.h>


#include "StringSerializer.h"
#include "AnyArrayUtil.h"

namespace ca {

class SpacePersister : public SpacePersister_Base
{
public:
	SpacePersister()
	{
		_trackedRevision = 0;
		// empty
	}

	virtual ~SpacePersister()
	{
		if( _spaceStore.isValid() )
		{
			_spaceStore->close();
		}
		if( _space != NULL )
		{
			_space->removeSpaceObserver( this );
		}
	}

	// ------ ca.ISpaceObserver Methods ------ //

	void onSpaceChanged( ca::ISpaceChanges* changes )
	{
		_spaceChanges.push_back( changes );
	}

	void insertObjectCache( co::IService* obj, co::uint32 id )
	{
		_objectIdCache.insert( ObjectIdMap::value_type( obj, id ) );
	}

	void insertNewObject( co::IService* obj )
	{
		_addedObjects.insert( obj );
	}

	void setUpdateList( const std::string& updateList )
	{
		_updateList = updateList;
	}

	// ------ ca.ISpacePersister Methods ------ //

	void initialize( co::IObject* rootObject )
	{
		assert( !_space );
		if( !_spaceStore.isValid() )
		{
			throw co::IllegalStateException("space file was not set, could not setup");
		}

		_spaceStore->open();

		try
		{
			_spaceStore->beginChanges();
			saveObject( rootObject );
			_spaceStore->setRootObject( getObjectId(rootObject) );

			co::Range<const std::string> updates = _model->getUpdates();
			std::stringstream updateList;

			for( int i = 0; i < updates.getSize(); i++)
			{
				updateList << updates[0] << ";";
			}

			_updateList = updateList.str();
			_spaceStore->commitChanges( _updateList );
		}
		catch( ... )
		{
			_spaceStore->discardChanges();
			_spaceStore->close();
			throw;
		}
		
		co::IObject* spaceObj = co::newInstance( "ca.Space" );
		_space = spaceObj->getService<ca::ISpace>();

		spaceObj->setService( "universe", _universe.get() );
		
		_space->setRootObject( rootObject );
		_space->notifyChanges();

		_trackedRevision = 1;

		assert( _spaceStore->getLatestRevision() == 1 );

		_space->addSpaceObserver( this );

		_spaceStore->close();

	}

	ca::ISpace* getSpace()
	{
		return _space.get();
	}

	void restore()
	{
		assert( !_space );
		if( !_spaceStore.isValid() )
		{
			throw co::IllegalStateException("space store was not set, can't restore a space");
		}
		_spaceStore->open();
		co::uint32 latestRevision = _spaceStore->getLatestRevision();
		_spaceStore->close();

		restoreRevision( latestRevision );
		
	}

	void restoreRevision( co::uint32 revision )
	{
		assert( !_space );
		if( !_spaceStore.isValid() )
		{
			throw co::IllegalStateException("space store was not set, can't restore a space");
		}

		_spaceStore->open();

		if( _spaceStore->getLatestRevision() == 0 )
		{
			_spaceStore->close();
			throw co::IllegalArgumentException("empty space store");
		}
		_spaceStore->close();

		try
		{
			clear();
			_trackedRevision = revision;
			restoreLua( _trackedRevision );
			_space->addSpaceObserver( this );

		}
		catch( ... )
		{
			_spaceStore->close();
			throw;
		}
		
	}

	void save()
	{
		if( _trackedRevision != _spaceStore->getLatestRevision() )
		{
			CORAL_THROW( ca::IOException, "Attempt to save changes in a intermediary revision" );
		}

		co::RefPtr<ca::ISpaceChanges> current;
		for( int i = 0; i < _spaceChanges.size(); i++ )
		{
			current = _spaceChanges[ i ];

			cacheChanges( current.get() );
		}

		std::vector<std::string> fieldNames;
		std::vector<std::string> values;
		
		_spaceStore->open();
		try
		{
			_spaceStore->beginChanges();

			for( ObjectSet::iterator it = _addedObjects.begin(); it != _addedObjects.end(); it++ )
			{
				co::IService* service = *it;
				IObject* object = service->getProvider();

				if( static_cast<IObject*>( service ) == object )
				{
					saveObject( object );

					if( object == _space->getRootObject() )
					{
						_spaceStore->setRootObject( getObjectId( object ) );
					}
				}
				else
				{
					if( getObjectId( service ) == 0 )
					{
						co::IPort* facet = service->getFacet();
						co::uint32 providerId = getObjectId( object );
						saveService( service, facet, providerId );

						ChangeSetCache::iterator itCache = _changeCache.find( object );

						bool objInserted = ( itCache != _changeCache.end() );

						ChangeSet &objChangeSet = ( objInserted ) ? itCache->second : ChangeSet();

						Change change;
						change.member = facet;
						change.newValue = *it;
						objChangeSet.insert(  change );

						if( !objInserted )
						{
							_changeCache.insert( ChangeSetCache::value_type( object, objChangeSet ) );
						}
					}
				}
				
			}

			for( ChangeSetCache::iterator it = _changeCache.begin(); it != _changeCache.end(); it++ )
			{
				co::uint32 objectId = getObjectId( it->first );

				for( ChangeSet::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++ )
				{
					
					std::string valueStr;
					if( it2->newValue.getKind() == co::TK_ARRAY )
					{
						co::IType* elementType = it2->newValue.getType();

						if( elementType->getKind() == co::TK_INTERFACE )
						{
							//RefVector value. get database ids;
							const co::RefVector<co::IService>& services = it2->newValue.get<const co::RefVector<co::IService>&>();

							std::vector<co::uint32> serviceIds;
							for( int i = 0; i < services.size(); i++ )
							{
								serviceIds.push_back( getObjectId( services[i].get() ) );
							}

							co::Any serviceIdsAny;
							serviceIdsAny.set< const std::vector< co::uint32 >& >( serviceIds );

							_serializer.toString( serviceIdsAny, valueStr );
							valueStr.insert( 0, "#" );
						}
						else
						{
							_serializer.toString( it2->newValue, valueStr );
						}
					}
					else if( it2->newValue.getKind() == co::TK_INTERFACE )
					{
						co::IService* service = it2->newValue.get<co::IService*>();
						if( service == NULL)
						{
							valueStr = "nil";
						}
						else
						{
							_serializer.toString( getObjectId( service ), valueStr );
							valueStr.insert( 0, "#" );
						}
					}
					else
					{
						_serializer.toString( it2->newValue, valueStr );
					}

					fieldNames.push_back( it2->member->getName() );
					values.push_back( valueStr );

				}

				_spaceStore->addValues( objectId, fieldNames, values );
				fieldNames.clear();
				values.clear();
			}
			_spaceStore->commitChanges( _updateList );
			_spaceStore->close();
			_trackedRevision++;
		}
		catch( ... )
		{
			_spaceStore->discardChanges();
			_spaceStore->close();
			throw;
		}


		_addedObjects.clear();
		_changeCache.clear();
		_spaceChanges.clear();
	}

protected:

	ca::ISpaceStore* getStoreService()
	{
		return _spaceStore.get();
	}

	void setStoreService( ca::ISpaceStore* spaceStore )
	{
		assert( spaceStore );
		_spaceStore = spaceStore;
	}

	ca::IUniverse* getUniverseService()
	{
		return _universe.get();
	}

	void setUniverseService( ca::IUniverse* universe )
	{
		assert( universe );
		_universe = universe;
		_model = static_cast<ca::IModel*>( _universe->getProvider()->getService( "model" ) );
		_serializer.setModel( _model );
	}

private:

	void restoreLua( co::uint32 revision )
	{
		const std::string& script = "ca.SpaceLoader";
		const std::string& function = "";

		co::Range<const co::Any> results;

		co::IObject* spaceObj = co::newInstance( "ca.Space" );
		spaceObj->setService( "universe", _universe.get() );
		_space = spaceObj->getService<ca::ISpace>();

		co::Any args[5];
		args[0].set<ca::ISpace*>( getSpace() );
		args[1].set<ca::ISpaceStore*>( _spaceStore.get() );
		args[2].set<ca::IModel*>( _model );
		args[3].set<co::uint32>( revision );
		args[4].set<ca::ISpaceLoader*>( this );

		co::getService<lua::IState>()->callFunction( script, function,
			co::Range<const co::Any>( args, CORAL_ARRAY_LENGTH( args ) ),
			results );

		_space->addSpaceObserver( this );
		_space->notifyChanges();


	}

	//save functions

	void saveService( co::RefPtr<co::IService> obj, co::IPort* port, co::uint32 providerId )
	{
		co::uint32 objId = getObjectId( obj.get() );
		if(objId != 0)
		{
			return;
		}

		co::IInterface* type = port->getType();

		objId = _spaceStore->addService( type->getFullName(), providerId );
		insertObjectCache(obj.get(), objId);

		co::RefVector<co::IField> fields;
		_model->getFields(type, fields);
						
		int j = 0;
		std::vector<std::string> fieldNames;
		std::vector<std::string> values;
		std::string fieldValueStr;
		std::vector<co::int32> refs;

		for( int i = 0; i < fields.size(); i++ ) 
		{
			co::IField* field = fields[i].get();
			
			co::IReflector* reflector = field->getOwner()->getReflector();
			co::Any fieldValue;

			reflector->getField(obj.get(), field, fieldValue);

			co::TypeKind tk = fieldValue.getKind();

			if( tk == co::TK_ARRAY )
			{
				co::IType* arrayType = static_cast<co::IArray*>( field->getType() )->getElementType();

				if( arrayType->getKind() == co::TK_INTERFACE )
				{
					co::Range<co::IService* const> services = fieldValue.get<co::Range<co::IService* const>>();
					
					refs.clear();
					while( !services.isEmpty() )
					{
						co::IService* const serv = services.getFirst();
						saveObject( serv->getProvider() );
						
						co::uint32 refId = getObjectId( serv );
						
						refs.push_back( refId );
						services.popFirst();
					
					}
					co::Any refsValue;
					refsValue.set<const std::vector<co::int32>&>( refs );
					_serializer.toString( refsValue, fieldValueStr );
					fieldValueStr.insert(0, "#");
				}
				else
				{
					_serializer.toString( fieldValue, fieldValueStr );
				}
				
			}
			else if ( tk == co::TK_INTERFACE )
			{
				co::IService* const service = fieldValue.get<co::IService* const>();
				if( service == NULL )
				{
					fieldValueStr = "nil";
				}
				else
				{
					saveObject( service->getProvider() );
					_serializer.toString( getObjectId(service), fieldValueStr );
					fieldValueStr.insert(0, "#");
				}
				
			}
			else
			{
				_serializer.toString( fieldValue, fieldValueStr );
			}

			fieldNames.push_back( field->getName() );
			values.push_back( fieldValueStr );
		}
		_spaceStore->addValues( objId, fieldNames, values );
	}

	void saveObject(co::RefPtr<co::IObject> object)
	{
		co::uint32 objId = getObjectId( object.get() );
		if( objId != 0 )
		{
			return;
		}

		co::IComponent* component = object->getComponent();

		objId = _spaceStore->addObject( component->getFullName() );

		insertObjectCache( object.get(), objId );

		co::Range<co::IPort* const> ports = component->getPorts();

		std::vector<std::string> values;
		std::vector<std::string> fieldNames;

		for( int i = 0; i < ports.getSize(); i++ )
		{
			co::IPort* port = (ports[i]);
			
			co::IService* service = object->getServiceAt( port );

			std::string refStr;
			if(port->getIsFacet())
			{
				saveService( service, port, objId );

				_serializer.toString( getObjectId( service ), refStr);

				refStr.insert(0, "#");

			}
			else
			{
				saveObject( service->getProvider() );
				_serializer.toString( getObjectId( service ), refStr);
				refStr.insert(0, "#");
			}
			
			values.push_back( refStr );
			fieldNames.push_back( port->getName() );
			
		}
		_spaceStore->addValues( objId, fieldNames, values );

	}

	co::uint32 getObjectId( co::IService* obj )
	{
		ObjectIdMap::iterator it = _objectIdCache.find(obj);

		if(it != _objectIdCache.end())
		{
			return it->second;
		}

		return 0;
	}

	void clear()
	{
		_objectIdCache.clear();

		if( _space != NULL )
		{
			_space->removeSpaceObserver( this );
		}
	}

	void cacheChanges( ca::ISpaceChanges* changes )
	{
		int addedObjectsSize = changes->getAddedObjects().getSize();

		for( int j = 0; j < addedObjectsSize; j++ )
		{
			_addedObjects.insert( changes->getAddedObjects()[j] );
		}

		Change change;
		ChangeSet changeSet;

		for( int j = 0; j < changes->getChangedObjects().getSize(); j++ )
		{
			ca::IObjectChanges * objectChanges = changes->getChangedObjects()[j];

			// changes have been made on an added object, there's no need to register these changes.
			// updating the object is enough
			ObjectSet::iterator objIt = _addedObjects.find( objectChanges->getObject() );
			if( objIt != _addedObjects.end() )
			{
				_addedObjects.insert( objIt, objectChanges->getObject() );
				continue;
			}

			ChangeSetCache::iterator it = _changeCache.find( objectChanges->getObject() );

			bool objInserted = ( it != _changeCache.end() );

			changeSet.clear();
			ChangeSet &objChangeSet = ( objInserted ) ? it->second : changeSet;

			for( int k = 0; k < objectChanges->getChangedConnections().getSize(); k++ )
			{
				ca::ChangedConnection changedConn = objectChanges->getChangedConnections()[k];

				change.member = changedConn.receptacle.get();
				change.newValue = changedConn.current.get();
				objChangeSet.insert( change );
			}

			if( !objInserted )
			{
				_changeCache.insert( ChangeSetCache::value_type( objectChanges->getObject(), objChangeSet ) );
			}

			for( int k = 0; k < objectChanges->getChangedServices().getSize(); k++ )
			{
				ca::IServiceChanges* changedServ = objectChanges->getChangedServices()[k];

				ChangeSetCache::iterator it = _changeCache.find( changedServ->getService() );

				bool servInserted = ( it != _changeCache.end() );

				changeSet.clear();
				ChangeSet &serviceChangeSet = ( servInserted ) ? it->second : changeSet;

				for( int l = 0; l < changedServ->getChangedValueFields().getSize(); l++ )
				{
					change.member = changedServ->getChangedValueFields()[l].field.get();
					change.newValue =  changedServ->getChangedValueFields()[l].current;
					serviceChangeSet.insert( change );
				}

				for( int l = 0; l < changedServ->getChangedRefFields().getSize(); l++ )
				{
					change.member = changedServ->getChangedRefFields()[l].field.get();
					change.newValue =  changedServ->getChangedRefFields()[l].current.get();
					serviceChangeSet.insert( change );
				}

				for( int l = 0; l < changedServ->getChangedRefVecFields().getSize(); l++ )
				{
					change.member = changedServ->getChangedRefVecFields()[l].field.get();

					change.newValue.set< const co::RefVector<co::IService>& >( changedServ->getChangedRefVecFields()[l].current );
					serviceChangeSet.insert( change );
				}

				if( !servInserted )
				{
					_changeCache.insert( ChangeSetCache::value_type( changedServ->getService(), serviceChangeSet ) );
				}
			}

			for( int j = 0; j < changes->getRemovedObjects().getSize(); j++ )
			{
				co::IObject* currentObject = changes->getRemovedObjects()[j];

				// currentObject was removed from the space graph, if it was present on previous changes,
				// these can be ignored

				_addedObjects.erase( currentObject );
				_changeCache.erase( currentObject );

				co::Range<co::IPort* const> ports = currentObject->getComponent()->getPorts();

				for( int k = 0; k < ports.getSize(); k++ )
				{
					_changeCache.erase( currentObject->getServiceAt( ports[k] ) );
				}
			}
		}
	}

private:
	typedef std::map<const std::string, std::string> FieldValueMap;
	typedef std::map<co::IService*, co::uint32> ObjectIdMap;

	typedef struct Change
	{
	public:
		co::IMember* member;
		co::Any newValue;

		bool operator< ( const Change& change ) const
		{
			return member < change.member;
		}

		bool operator== ( const Change& change ) const
		{
			return member == change.member;
		}

		Change& operator=( const Change& change )
		{
			member = change.member;
			newValue = change.newValue;
		}
	};

	typedef std::set<Change> ChangeSet;
	typedef std::set<co::IService*> ObjectSet;

	typedef std::map<co::IService*, ChangeSet> ChangeSetCache;

private:

	co::RefPtr<ca::ISpace> _space;
	co::RefPtr<ca::IUniverse> _universe;
	co::RefPtr<ca::ISpaceStore> _spaceStore;

	StringSerializer _serializer;
	ca::IModel* _model;

	co::RefVector<ca::ISpaceChanges> _spaceChanges;

	co::uint32 _trackedRevision;
	std::string _updateList;

	ObjectIdMap _objectIdCache;

	ChangeSetCache _changeCache;
	ObjectSet _addedObjects;

};

CORAL_EXPORT_COMPONENT( SpacePersister, SpacePersister );

} // namespace ca
