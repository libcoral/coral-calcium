/*
 * Calcium - Domain Model Framework
 * See copyright notice in LICENSE.md
 */
#include "ERMSpace.h"

#include <gtest/gtest.h>

#include <co/Coral.h>
#include <co/IObject.h>
#include <ca/ISpaceSaver.h>
#include <co/RefPtr.h>
#include <co/Coral.h>
#include <co/IObject.h>
#include <co/IllegalStateException.h>
#include <co/IllegalArgumentException.h>
#include <erm/IModel.h>
#include <erm/IRelationship.h>
#include <erm/IEntity.h>

#include <ca/IModel.h>
#include <ca/ISpace.h>
#include <ca/INamed.h>
#include <ca/IUniverse.h>
#include <ca/ModelException.h>
#include <ca/IDBConnection.h>
#include <ca/IResultSet.h>
#include <ca/NoSuchObjectException.h>
#include <cstdio>
#include <sqlite3.h>


class SpaceSaverTest : public ERMSpace {};

#if 0
inline erm::Multiplicity mult( co::int32 min, co::int32 max )
{
	erm::Multiplicity m;
	m.min = min; m.max = max;
	return m;
}

TEST_F( SpaceSaverTest, testNewFileSetup )
{
	int modelVersion = 1;

	co::RefPtr<co::IObject> spaceObj = co::newInstance( "ca.Space" );
	ca::ISpace* space = spaceObj->getService<ca::ISpace>();
	
	startWithExtendedERM();
	_relAB->setMultiplicityB( mult( 1, 2 ) );
	_relBC->setMultiplicityA( mult( 3, 4 ) );
	_relBC->setMultiplicityB( mult( 5, 6 ) );
	_relCA->setMultiplicityA( mult( 7, 8 ) );
	_relCA->setMultiplicityB( mult( 9, 0 ) );

	co::RefPtr<co::IObject> universeObj = co::newInstance( "ca.Universe" );
	ca::IUniverse* universe = universeObj->getService<ca::IUniverse>();
	spaceObj->setService( "universe", universe );

	universeObj->setService("model", _model.get());
	
	space->setRootObject(_erm->getProvider());
	
	co::RefPtr<co::IObject> obj = co::newInstance( "ca.SpaceSaverSQLite3" );
	co::RefPtr<ca::ISpaceSaver> spaceSav = obj->getService<ca::ISpaceSaver>();

	std::string fileName = "SimpleSpaceSave.db";
	
	spaceSav->setSpace( space );

	remove( fileName.c_str() );

	ca::INamed* file = (obj->getService<ca::INamed>());
	file->setName( fileName );
	
	spaceSav->setSpace( space );

	ASSERT_NO_THROW(spaceSav->setup());

	ca::ISpace * spaceRestored = spaceSav->getVersion(1);

	co::IObject* objRest = spaceRestored->getRootObject();

	erm::IModel* erm = objRest->getService<erm::IModel>();
	ASSERT_TRUE( erm != NULL );

	co::Range<erm::IEntity* const> entities = erm->getEntities();
	ASSERT_EQ( 3, entities.getSize() );

	EXPECT_EQ( "Entity A", entities[0]->getName() );
	EXPECT_EQ( "Entity B", entities[1]->getName() );
	EXPECT_EQ( "Entity C", entities[2]->getName() );

	co::Range<erm::IRelationship* const> rels = erm->getRelationships();
	ASSERT_EQ( 3, rels.getSize() );

	erm::IRelationship* rel = rels[0];
	EXPECT_EQ( "relation A-B", rel->getRelation() );
	EXPECT_EQ( entities[0], rel->getEntityA() );
	EXPECT_EQ( entities[1], rel->getEntityB() );
	EXPECT_EQ( 0, rel->getMultiplicityA().min );
	EXPECT_EQ( 0, rel->getMultiplicityA().max );
	EXPECT_EQ( 1, rel->getMultiplicityB().min );
	EXPECT_EQ( 2, rel->getMultiplicityB().max );

	rel = rels[1];
	EXPECT_EQ( "relation B-C", rel->getRelation() );
	EXPECT_EQ( entities[1], rel->getEntityA() );
	EXPECT_EQ( entities[2], rel->getEntityB() );
	EXPECT_EQ( 3, rel->getMultiplicityA().min );
	EXPECT_EQ( 4, rel->getMultiplicityA().max );
	EXPECT_EQ( 5, rel->getMultiplicityB().min );
	EXPECT_EQ( 6, rel->getMultiplicityB().max );

	rel = rels[2];
	EXPECT_EQ( "relation C-A", rel->getRelation() );
	EXPECT_EQ( entities[2], rel->getEntityA() );
	EXPECT_EQ( entities[0], rel->getEntityB() );
	EXPECT_EQ( 7, rel->getMultiplicityA().min );
	EXPECT_EQ( 8, rel->getMultiplicityA().max );
	EXPECT_EQ( 9, rel->getMultiplicityB().min );
	EXPECT_EQ( 0, rel->getMultiplicityB().max );
	
	//co::RefPtr<co::IObject> dbObj = co::newInstance( "ca.SQLiteDBConnection" );
	//
	//co::RefPtr<ca::INamed> named = dbObj->getService<ca::INamed>();
	//named->setName(fileName);

	//co::RefPtr<ca::IDBConnection> conn = dbObj->getService<ca::IDBConnection>();

	//FILE* dbFile = 0;
	//EXPECT_NO_THROW(dbFile = fopen(fileName.c_str(), "r"));
	//EXPECT_TRUE(dbFile != 0);
	//fclose(dbFile);

	//conn->open();

	//co::RefPtr<ca::IResultSet> resultSet = conn->executeQuery("SELECT name FROM sqlite_master WHERE type = 'table' and name <> 'sqlite_sequence' ORDER BY name");

	//char* tables[] = { "CALCIUM_MODEL",
	//				    "ENTITY",
	//					"FIELD",
	//					"FIELD_VALUES",
	//					"OBJECT"
	//				 };

	//int i = 0;
	//std::string resultI;
	//while (resultSet->next())
	//{
	//	resultI = resultSet->getValue(0);
	//	EXPECT_TRUE(strcmp(resultI.c_str(), tables[i])==0);
	//	i++;
	//}
	//EXPECT_EQ( i, 5);

	//resultSet = conn->executeQuery("SELECT * FROM CALCIUM_MODEL");

	//EXPECT_TRUE(resultSet->next());

	//int caModelId = atoi(resultSet->getValue(0).c_str());

	//EXPECT_EQ(caModelId, 1);

	//////Model name
	//EXPECT_TRUE(strcmp(resultSet->getValue(1).c_str(), _model->getName().c_str()) == 0);

	////Model version implementation pending
	//int caModelVersion = atoi(resultSet->getValue(2).c_str());
	//EXPECT_EQ(caModelVersion, 1);

	//resultSet->finalize();

	//conn->close();


}

#endif